#include "gamewidget.h"
#include <QApplication>
#include <QCursor>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QFocusEvent>
#include <QSettings>
#include <QGuiApplication>
#include <algorithm>
#include <cmath>
#include <queue>
#include <limits>

namespace {
constexpr double Pi = 3.14159265358979323846;
const std::array<int,2> Magazine {{18,6}};
double length(QPointF p) { return std::hypot(p.x(),p.y()); }
QPointF direction(double a) { return {std::cos(a),std::sin(a)}; }
double dot(QPointF a,QPointF b) { return a.x()*b.x()+a.y()*b.y(); }
double normalizedAngle(double a) { return std::remainder(a,2*Pi); }
QSettings settings() { return QSettings(QSettings::IniFormat,QSettings::UserScope,"CG Lab","Grid Breach 2D"); }
}

GameWidget::GameWidget(QWidget* parent) : QWidget(parent) {
    setWindowTitle("GRID BREACH — 2D Grid FPS");
    setMinimumSize(1024,720); resize(1280,820);
    setFocusPolicy(Qt::StrongFocus); setMouseTracking(true);
    m_testMode = qEnvironmentVariableIsSet("GRIDBREACH_TEST") || QGuiApplication::platformName()=="offscreen";
    auto saved = settings();
    for (int i=0;i<3;++i) m_best[i]=saved.value(QString("best/%1").arg(i),0).toInt();
    m_sound.setMuted(saved.value("muted",false).toBool());
    // Every wall and passage occupies cells in a 24 x 24 integer map.
    for (int y=0;y<MapSize;++y) for (int x=0;x<MapSize;++x)
        m_map[y][x] = (x==0 || y==0 || x==MapSize-1 || y==MapSize-1) ? 1 : 0;
    for (int y=1;y<=16;++y) if (y!=4 && y!=5 && y!=12 && y!=13) m_map[y][8]=2;
    for (int y=7;y<=22;++y) if (y!=10 && y!=11 && y!=18 && y!=19) m_map[y][16]=2;
    for (int x=9;x<=22;++x) if (x!=12 && x!=13 && x!=20 && x!=21) m_map[8][x]=3;
    for (int x=1;x<=15;++x) if (x!=4 && x!=5 && x!=12 && x!=13) m_map[16][x]=3;
    for (QPoint pillar : {QPoint(12,3),QPoint(19,3),QPoint(3,10),QPoint(11,20),QPoint(20,13)})
        for (int y=0;y<2;++y) for (int x=0;x<2;++x) m_map[pillar.y()+y][pillar.x()+x]=1;
    updatePaths();
    m_clock.start(); m_lastTick=m_clock.elapsed();
    m_timer.setTimerType(Qt::PreciseTimer); m_timer.setInterval(16);
    connect(&m_timer,&QTimer::timeout,this,&GameWidget::tick); m_timer.start();
}
GameWidget::~GameWidget() { setMouseLook(false); }

double GameWidget::random(double low,double high) { return std::uniform_real_distribution<double>(low,high)(m_rng); }
int GameWidget::tile(int x,int y) const {
    if (x<0 || y<0 || x>=MapSize || y>=MapSize) return 1;
    return m_map[y][x];
}
bool GameWidget::walkable(QPointF p,double radius) const {
    for (int y=int(std::floor(p.y()-radius));y<=int(std::floor(p.y()+radius));++y)
        for (int x=int(std::floor(p.x()-radius));x<=int(std::floor(p.x()+radius));++x)
            if (tile(x,y)) return false;
    return true;
}
void GameWidget::moveActor(QPointF& p,QPointF delta,double radius) {
    int steps=std::max(1,int(std::ceil(length(delta)/0.12)));
    delta/=steps;
    for (int i=0;i<steps;++i) {
        QPointF candidate=p+QPointF(delta.x(),0);
        if (walkable(candidate,radius)) p=candidate;
        candidate=p+QPointF(0,delta.y());
        if (walkable(candidate,radius)) p=candidate;
    }
}
double GameWidget::castRay(QPointF pos,double angle,double limit,int* side,int* wall,double* u) const {
    QPointF ray=direction(angle);
    int x=int(std::floor(pos.x())), y=int(std::floor(pos.y()));
    int stepX=ray.x()<0?-1:1, stepY=ray.y()<0?-1:1;
    double dx=std::abs(ray.x())<1e-10?1e30:std::abs(1/ray.x());
    double dy=std::abs(ray.y())<1e-10?1e30:std::abs(1/ray.y());
    double nextX=(stepX>0?x+1-pos.x():pos.x()-x)*dx;
    double nextY=(stepY>0?y+1-pos.y():pos.y()-y)*dy;
    double distance=0; int hitSide=0;
    for (int count=0;count<MapSize*4 && !tile(x,y);++count) {
        if (nextX<nextY) { distance=nextX;nextX+=dx;x+=stepX;hitSide=0; }
        else { distance=nextY;nextY+=dy;y+=stepY;hitSide=1; }
        if (distance>=limit) { distance=limit;break; }
    }
    if (side) *side=hitSide;
    if (wall) *wall=tile(x,y);
    if (u) { double coordinate=hitSide==0?pos.y()+distance*ray.y():pos.x()+distance*ray.x(); *u=coordinate-std::floor(coordinate); }
    return std::min(limit,distance);
}
bool GameWidget::clearLine(QPointF a,QPointF b) const {
    QPointF delta=b-a; double distance=length(delta);
    return distance<0.001 || castRay(a,std::atan2(delta.y(),delta.x()),distance+0.05)>=distance;
}
void GameWidget::updatePaths() {
    for (auto& row:m_path) row.fill(-1);
    QPoint start(int(m_player.x()),int(m_player.y()));
    if (tile(start.x(),start.y())) return;
    std::queue<QPoint> queue;queue.push(start);m_path[start.y()][start.x()]=0;
    const std::array<QPoint,4> moves {{QPoint(1,0),QPoint(-1,0),QPoint(0,1),QPoint(0,-1)}};
    while (!queue.empty()) {
        QPoint p=queue.front();queue.pop();
        for (auto d:moves) {
            QPoint n=p+d;
            if (tile(n.x(),n.y()) || m_path[n.y()][n.x()]>=0) continue;
            m_path[n.y()][n.x()]=m_path[p.y()][p.x()]+1;queue.push(n);
        }
    }
}
int GameWidget::aliveEnemies() const { return int(std::count_if(m_enemies.begin(),m_enemies.end(),[](const Enemy& e){return e.alive;})); }
int GameWidget::difficultyBest() const { return m_best[m_difficulty]; }
void GameWidget::setMouseLook(bool enabled) {
    m_mouseLook=enabled;
    if (m_testMode) return;
    setCursor(enabled?Qt::BlankCursor:Qt::ArrowCursor);
    if (enabled) QCursor::setPos(mapToGlobal(viewportRect().center().toPoint()));
}
void GameWidget::startGame() {
    m_player={3.5,3.5};m_angle=0;m_time=0;m_wave=0;m_score=0;m_kills=0;m_shots=0;m_hits=0;
    m_health=m_maxHealth=(m_difficulty==0?125:(m_difficulty==2?90:100));m_shield=25;m_stamina=100;
    m_ammo={{18,6}};m_reserve={{90,24}};m_weapon=0;
    m_damageScale=m_reloadScale=1;m_fireCooldown=m_reloadTimer=m_dashCooldown=0;
    m_damageFlash=m_muzzleFlash=m_hitMarker=m_recoil=m_bob=0;
    m_pathTimer=0;m_newBest=false;m_nextEnemyId=1;
    m_enemies.clear();m_projectiles.clear();m_effects.clear();m_pickups.clear();m_barrels.clear();m_keys.clear();m_firing=false;
    for (QPointF p:{QPointF(6.5,6.5),QPointF(13.5,10.5),QPointF(20.5,20.5),QPointF(5.5,19.5)}) m_barrels.push_back({p,true});
    m_pickups.push_back({Pickup::Ammo,{2.5,6.5}});
    m_pickups.push_back({Pickup::Health,{10.5,3.5}});
    m_pickups.push_back({Pickup::Shield,{20.5,6.5}});
    m_pickups.push_back({Pickup::Ammo,{5.5,20.5}});
    m_pickups.push_back({Pickup::Health,{20.5,21.5}});
    m_state=State::Playing;m_sound.stopAll();updatePaths();startWave();
    m_lastTick=m_clock.elapsed();setFocus();setMouseLook(true);update();
}
void GameWidget::startWave() {
    ++m_wave;m_enemies.clear();m_projectiles.clear();
    m_reserve[0]=std::min(180,m_reserve[0]+(m_wave==1?0:36));
    m_reserve[1]=std::min(48,m_reserve[1]+(m_wave==1?0:8));
    m_shield=std::min(75.0,m_shield+10);m_health=std::min(m_maxHealth,m_health+10);
    updatePaths();
    std::vector<QPointF> cells;
    for (int y=1;y<MapSize-1;++y) for (int x=1;x<MapSize-1;++x)
        if (!tile(x,y) && m_path[y][x]>=0 && length(QPointF(x+.5,y+.5)-m_player)>5) cells.push_back({x+.5,y+.5});
    std::shuffle(cells.begin(),cells.end(),m_rng);
    int count=(m_wave==5?9:3+m_wave*2)+(m_difficulty==2?2:0);
    for (int i=0;i<count && !cells.empty();++i) {
        Enemy e;e.id=m_nextEnemyId++;e.pos=cells.back();cells.pop_back();
        e.kind=m_wave==5 && i==0?Enemy::Sentinel:(i%4==3 && m_wave>=2?Enemy::Bomber:(i%3==2?Enemy::Runner:Enemy::Guard));
        if (m_wave==1 && i==0) {e.pos={6.5,3.5};e.kind=Enemy::Guard;}
        e.maxHealth=e.health=(e.kind==Enemy::Sentinel?650:e.kind==Enemy::Runner?45:e.kind==Enemy::Bomber?50:65)*(m_difficulty==2?1.2:1.0);
        e.attackTimer=random(.8,2);m_enemies.push_back(e);
    }
    m_banner=m_wave==5?"FINAL WAVE / SENTINEL ONLINE":QString("WAVE %1 / SECTOR BREACH").arg(m_wave);
    m_bannerTimer=3;m_sound.play("wave");
}
void GameWidget::finishGame(bool won) {
    if (m_state!=State::Playing) return;
    m_state=won?State::Won:State::Lost;m_firing=false;m_keys.clear();m_reloadTimer=0;
    setMouseLook(false);m_sound.stopAll();m_sound.play(won?"victory":"defeat");
    if (won) m_score+=2000+int(std::max(0.0,900-m_time))*2;
    m_newBest=m_score>m_best[m_difficulty];
    if (m_newBest) {m_best[m_difficulty]=m_score;auto saved=settings();saved.setValue(QString("best/%1").arg(m_difficulty),m_score);}
    update();
}
void GameWidget::pauseGame() {
    if (m_state!=State::Playing) return;
    m_state=State::Paused;m_firing=false;m_keys.clear();setMouseLook(false);m_sound.stopAll();update();
}
void GameWidget::resumeGame() {
    if (m_state!=State::Paused) return;
    m_state=State::Playing;m_lastTick=m_clock.elapsed();setFocus();setMouseLook(true);update();
}
void GameWidget::activateAction(int index) {
    if (m_state==State::Menu) {
        if (index==0) startGame();
        else if (index>=1 && index<=3) m_difficulty=index-1;
    } else if (m_state==State::Paused) {
        if (index==0) resumeGame();else if (index==1) startGame();else if(index==2) {m_state=State::Menu;setMouseLook(false);}
    } else if (m_state==State::Upgrade) {
        if (index<0 || index>2) return;
        if(index==0) m_damageScale+=.2;
        else if(index==1) {m_maxHealth+=25;m_health=std::min(m_maxHealth,m_health+50);}
        else m_reloadScale=std::max(.45,m_reloadScale*.8);
        m_state=State::Playing;m_keys.clear();m_firing=false;startWave();m_lastTick=m_clock.elapsed();setFocus();setMouseLook(true);
    } else if (m_state==State::Won || m_state==State::Lost) {
        if(index==0) startGame();else if(index==1) {m_state=State::Menu;setMouseLook(false);}
    }
    update();
}
void GameWidget::addEffect(QPointF p,QColor color,double size) { m_effects.push_back({p,.45,.45,color,size}); }
void GameWidget::hurtPlayer(int damage) {
    if (m_state!=State::Playing || damage<=0) return;
    double scaled=damage*(m_difficulty==0?.65:(m_difficulty==2?1.25:1.0));
    double absorbed=std::min(m_shield,scaled*.65);m_shield-=absorbed;m_health=std::max(0.0,m_health-(scaled-absorbed));
    m_damageFlash=.32;m_sound.play("hurt");
    if(m_health<=0) finishGame(false);
}
void GameWidget::hurtEnemy(int index,double damage) {
    if(index<0 || index>=int(m_enemies.size())) return;
    Enemy& e=m_enemies[index];if(!e.alive) return;
    e.health-=damage;e.hitFlash=.14;m_hitMarker=.12;
    if(e.health>0) return;
    e.alive=false;++m_kills;
    m_score+=(e.kind==Enemy::Sentinel?1500:e.kind==Enemy::Bomber?180:e.kind==Enemy::Runner?140:100)+m_wave*10;
    addEffect(e.pos,e.kind==Enemy::Bomber?QColor("#ff704e"):QColor("#70e5d5"),e.kind==Enemy::Sentinel?1.8:.7);
    if(random(0,1)<.5) m_pickups.push_back({random(0,1)<.7?Pickup::Ammo:Pickup::Health,e.pos});
}
void GameWidget::explode(QPointF p,double radius,double damage,bool hurtSelf) {
    addEffect(p,QColor("#ffad55"),radius);m_sound.play("explosion");
    for(int i=0;i<int(m_enemies.size());++i) {
        double distance=length(m_enemies[i].pos-p);
        if(m_enemies[i].alive && distance<radius && clearLine(p,m_enemies[i].pos)) hurtEnemy(i,damage*(1-.6*distance/radius));
    }
    if(hurtSelf && length(m_player-p)<radius && clearLine(p,m_player)) hurtPlayer(int(damage*.4*(1-length(m_player-p)/radius)));
    for(auto& barrel:m_barrels) if(barrel.active && length(barrel.pos-p)<radius && clearLine(p,barrel.pos)) {
        barrel.active=false;explode(barrel.pos,2.8,160,true);
    }
}
void GameWidget::shoot() {
    if(m_state!=State::Playing || m_fireCooldown>0 || m_reloadTimer>0) return;
    if(m_ammo[m_weapon]<=0) {reload();return;}
    --m_ammo[m_weapon];++m_shots;m_fireCooldown=m_weapon==0?.19:.72;m_muzzleFlash=.085;m_recoil=1;
    m_sound.play(m_weapon==0?"rifle":"shotgun");
    bool hit=false;
    int pellets=m_weapon==0?1:7;
    for(int pellet=0;pellet<pellets;++pellet) {
        double angle=m_angle+(m_weapon==0?0:(pellet-3)*.024);
        QPointF ray=direction(angle);double nearest=castRay(m_player,angle,24);
        int enemyIndex=-1,barrelIndex=-1;
        auto distanceTo=[&](QPointF pos,double radius) {
            QPointF relative=pos-m_player;double along=dot(relative,ray),perp2=dot(relative,relative)-along*along;
            if(along<=0 || perp2>radius*radius) return 1e9;
            return std::max(0.0,along-std::sqrt(std::max(0.0,radius*radius-perp2)));
        };
        for(int i=0;i<int(m_enemies.size());++i) if(m_enemies[i].alive) {
            double distance=distanceTo(m_enemies[i].pos,m_enemies[i].kind==Enemy::Sentinel?.55:.30);
            if(distance<nearest) {nearest=distance;enemyIndex=i;}
        }
        for(int i=0;i<int(m_barrels.size());++i) if(m_barrels[i].active) {
            double distance=distanceTo(m_barrels[i].pos,.28);
            if(distance<nearest) {nearest=distance;barrelIndex=i;enemyIndex=-1;}
        }
        if(enemyIndex>=0) {hurtEnemy(enemyIndex,(m_weapon==0?34:15)*m_damageScale);hit=true;}
        else if(barrelIndex>=0) {m_barrels[barrelIndex].active=false;explode(m_barrels[barrelIndex].pos,2.8,160,true);hit=true;}
        else addEffect(m_player+ray*std::max(0.0,nearest-.05),QColor("#ffd491"),.10);
    }
    if(hit) {++m_hits;m_hitMarker=.16;m_sound.play("hit");}
}
void GameWidget::reload() {
    if(m_state!=State::Playing || m_reloadTimer>0 || m_ammo[m_weapon]>=Magazine[m_weapon] || m_reserve[m_weapon]<=0) return;
    m_reloadDuration=(m_weapon==0?1.35:1.8)*m_reloadScale;m_reloadTimer=m_reloadDuration;m_sound.play("reload");
}
void GameWidget::switchWeapon(int weapon) {
    if(m_state!=State::Playing || weapon<0 || weapon>1 || weapon==m_weapon) return;
    m_weapon=weapon;m_reloadTimer=0;m_fireCooldown=.22;m_recoil=.6;
}
void GameWidget::tick() {
    qint64 now=m_clock.elapsed();double dt=std::min(.05,(now-m_lastTick)/1000.0);m_lastTick=now;
    advance(std::max(0.0,dt));update();
}
void GameWidget::advance(double dt) {
    if(m_state!=State::Playing || dt<=0) return;
    m_time+=dt;
    auto decay=[dt](double& value){value=std::max(0.0,value-dt);};
    decay(m_fireCooldown);decay(m_dashCooldown);decay(m_damageFlash);decay(m_hitMarker);decay(m_muzzleFlash);decay(m_bannerTimer);
    m_recoil=std::max(0.0,m_recoil-dt*6);
    if(m_reloadTimer>0) {
        m_reloadTimer=std::max(0.0,m_reloadTimer-dt);
        if(m_reloadTimer==0) {int count=std::min(Magazine[m_weapon]-m_ammo[m_weapon],m_reserve[m_weapon]);m_ammo[m_weapon]+=count;m_reserve[m_weapon]-=count;}
    }
    if(m_keys.contains(Qt::Key_Left)) m_angle-=dt*1.9;
    if(m_keys.contains(Qt::Key_Right)) m_angle+=dt*1.9;
    m_angle=normalizedAngle(m_angle);
    QPointF velocity;
    if(m_keys.contains(Qt::Key_W) || m_keys.contains(Qt::Key_Up)) velocity+=direction(m_angle);
    if(m_keys.contains(Qt::Key_S) || m_keys.contains(Qt::Key_Down)) velocity-=direction(m_angle);
    if(m_keys.contains(Qt::Key_A)) velocity+=direction(m_angle-Pi/2);
    if(m_keys.contains(Qt::Key_D)) velocity+=direction(m_angle+Pi/2);
    bool moving=length(velocity)>.1;
    bool sprint=moving && m_keys.contains(Qt::Key_Shift) && m_stamina>2;
    m_stamina=std::clamp(m_stamina+dt*(sprint?-28:20),0.0,100.0);
    if(moving) {moveActor(m_player,velocity/length(velocity)*(sprint?4.2:2.8)*dt,.22);m_bob+=dt*(sprint?13:9);}
    if(m_firing) shoot();
    if(m_state!=State::Playing) return;
    m_pathTimer-=dt;if(m_pathTimer<=0) {updatePaths();m_pathTimer=.3;}
    for(int i=0;i<int(m_enemies.size());++i) {
        Enemy& e=m_enemies[i];if(!e.alive) continue;
        decay(e.hitFlash);e.attackTimer-=dt;
        QPointF delta=m_player-e.pos;double distance=length(delta);
        bool visible=clearLine(e.pos,m_player);
        if(e.kind==Enemy::Bomber && distance<1.05 && visible) {QPointF pos=e.pos;hurtEnemy(i,10000);explode(pos,2.5,100,true);continue;}
        if(distance<.75 && e.attackTimer<=0 && visible) {hurtPlayer(e.kind==Enemy::Sentinel?22:12);e.attackTimer=.8;}
        else if((e.kind==Enemy::Guard || e.kind==Enemy::Sentinel) && visible && distance<10 && e.attackTimer<=0) {
            double angle=std::atan2(delta.y(),delta.x());int shots=e.kind==Enemy::Sentinel?3:1;
            for(int s=0;s<shots;++s) m_projectiles.push_back({e.pos,direction(angle+(s-(shots-1)/2.0)*.16)*(e.kind==Enemy::Sentinel?5.8:4.5),4,e.kind==Enemy::Sentinel?18:10});
            e.attackTimer=(e.kind==Enemy::Sentinel?.9:1.6)*(m_difficulty==2?.8:1.0);
        }
        if(m_state!=State::Playing) return;
        if(distance<.62 || ((e.kind==Enemy::Guard || e.kind==Enemy::Sentinel) && visible && distance>2.4 && distance<5.5)) continue;
        QPointF destination=m_player;
        if(!visible) {
            int x=int(e.pos.x()),y=int(e.pos.y());int best=m_path[y][x];destination=QPointF(x+.5,y+.5);
            for(QPoint d:{QPoint(1,0),QPoint(-1,0),QPoint(0,1),QPoint(0,-1)}) {
                int nx=x+d.x(),ny=y+d.y();if(tile(nx,ny)) continue;
                if(m_path[ny][nx]>=0 && (best<0 || m_path[ny][nx]<best)) {best=m_path[ny][nx];destination={nx+.5,ny+.5};}
            }
        }
        QPointF motion=destination-e.pos;double speed=e.kind==Enemy::Runner?2.05:e.kind==Enemy::Bomber?1.65:e.kind==Enemy::Sentinel?.9:1.2;
        if(length(motion)>.02) moveActor(e.pos,motion/length(motion)*speed*dt,.20);
    }
    for(auto& projectile:m_projectiles) {
        if(projectile.life<=0) continue;
        projectile.life-=dt;
        QPointF motion=projectile.velocity*dt;int steps=std::max(1,int(std::ceil(length(motion)/.10)));
        for(int i=0;i<steps;++i) {
            projectile.pos+=motion/steps;
            if(!walkable(projectile.pos,.06)) {projectile.life=0;break;}
            if(length(projectile.pos-m_player)<.30) {projectile.life=0;hurtPlayer(projectile.damage);break;}
        }
    }
    m_projectiles.erase(std::remove_if(m_projectiles.begin(),m_projectiles.end(),[](const Projectile& p){return p.life<=0;}),m_projectiles.end());
    for(auto& pickup:m_pickups) if(pickup.active && length(pickup.pos-m_player)<.55) {
        if(pickup.kind==Pickup::Health && m_health>=m_maxHealth) continue;
        if(pickup.kind==Pickup::Shield && m_shield>=75) continue;
        pickup.active=false;
        if(pickup.kind==Pickup::Ammo) {m_reserve[0]=std::min(180,m_reserve[0]+30);m_reserve[1]=std::min(48,m_reserve[1]+6);m_banner="AMMO RESUPPLY / +30 RIFLE +6 SHOTGUN";}
        else if(pickup.kind==Pickup::Health) {m_health=std::min(m_maxHealth,m_health+30);m_banner="MEDKIT / +30 HEALTH";}
        else {m_shield=std::min(75.0,m_shield+30);m_banner="ARMOR CELL / +30 SHIELD";}
        m_bannerTimer=1.5;m_sound.play("pickup");
    }
    for(auto& e:m_effects) e.life-=dt;
    m_effects.erase(std::remove_if(m_effects.begin(),m_effects.end(),[](const WorldEffect& e){return e.life<=0;}),m_effects.end());
    if(m_state==State::Playing && aliveEnemies()==0) {
        if(m_wave>=5) finishGame(true);
        else {m_state=State::Upgrade;m_firing=false;m_keys.clear();m_reloadTimer=0;setMouseLook(false);m_sound.stopAll();m_sound.play("wave");}
    }
}
void GameWidget::keyPressEvent(QKeyEvent* e) {
    if(e->isAutoRepeat()) return;
    if(e->key()==Qt::Key_M) {m_sound.setMuted(!m_sound.muted());auto saved=settings();saved.setValue("muted",m_sound.muted());update();return;}
    if(e->key()==Qt::Key_F) {isFullScreen()?showNormal():showFullScreen();return;}
    if(e->key()==Qt::Key_Escape) {
        if(m_state==State::Playing) pauseGame();else if(m_state==State::Paused) resumeGame();return;
    }
    if((e->key()==Qt::Key_Return || e->key()==Qt::Key_Enter) && m_state!=State::Playing) {activateAction(0);return;}
    if(m_state==State::Upgrade && e->key()>=Qt::Key_1 && e->key()<=Qt::Key_3) {activateAction(e->key()-Qt::Key_1);return;}
    if(m_state!=State::Playing) return;
    if(e->key()==Qt::Key_R) reload();
    else if(e->key()==Qt::Key_1 || e->key()==Qt::Key_2) switchWeapon(e->key()-Qt::Key_1);
    else if(e->key()==Qt::Key_Tab) m_showMap=!m_showMap;
    else if(e->key()==Qt::Key_Space && m_dashCooldown<=0 && m_stamina>=25) {
        QPointF dash=direction(m_angle);
        if(m_keys.contains(Qt::Key_A)) dash=direction(m_angle-Pi/2);
        else if(m_keys.contains(Qt::Key_D)) dash=direction(m_angle+Pi/2);
        else if(m_keys.contains(Qt::Key_S)) dash=-dash;
        moveActor(m_player,dash*1.7,.22);m_dashCooldown=2.5;m_stamina-=25;
    }
    m_keys.insert(e->key());
}
void GameWidget::keyReleaseEvent(QKeyEvent* e) {if(!e->isAutoRepeat()) m_keys.remove(e->key());}
void GameWidget::mousePressEvent(QMouseEvent* e) {
    if(e->button()!=Qt::LeftButton) return;
    if(m_state==State::Playing) {m_firing=true;shoot();}
    else for(int i=0;i<4;++i) if(actionRect(i).contains(e->position())) {activateAction(i);break;}
}
void GameWidget::mouseReleaseEvent(QMouseEvent* e) {if(e->button()==Qt::LeftButton) m_firing=false;}
void GameWidget::mouseMoveEvent(QMouseEvent* e) {
    if(m_state==State::Playing && m_mouseLook && !m_testMode) {
        QPoint center=viewportRect().center().toPoint();int dx=int(e->position().x())-center.x();
        if(dx) m_angle=normalizedAngle(m_angle+dx*.0032);
        if((e->position()-center).manhattanLength()>0) QCursor::setPos(mapToGlobal(center));
    } else update();
}
void GameWidget::resizeEvent(QResizeEvent*) {if(m_mouseLook) setMouseLook(true);}
void GameWidget::focusOutEvent(QFocusEvent* e) {if(!m_testMode) pauseGame();QWidget::focusOutEvent(e);}
