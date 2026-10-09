#include "gamewidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QCursor>
#include <QFont>
#include <QStringList>
#include <algorithm>
#include <cmath>

namespace {
constexpr double Pi = 3.14159265358979323846;
const QColor Ink(11, 17, 24), Panel(18, 28, 37), Edge(43, 61, 72);
const QColor White(225, 236, 239), Muted(126, 151, 163), Teal(91, 230, 202);
const QColor Amber(255, 192, 104), Red(255, 92, 103);

QFont font(int pixels, bool bold = false, bool mono = false) {
    QFont f(mono ? "Menlo" : "Avenir Next");
    f.setPixelSize(pixels);
    f.setWeight(bold ? QFont::Bold : QFont::Normal);
    return f;
}
void label(QPainter& p, QRectF r, const QString& s, int size, QColor color = White,
           bool bold = false, int align = Qt::AlignLeft | Qt::AlignVCenter, bool mono = false) {
    p.setFont(font(size, bold, mono)); p.setPen(color); p.drawText(r, align, s);
}
void box(QPainter& p, QRectF r, QColor fill = Panel, QColor border = Edge, int radius = 10) {
    p.setPen(QPen(border, 1)); p.setBrush(fill); p.drawRoundedRect(r, radius, radius);
}
void bar(QPainter& p, QRectF r, double fraction, QColor color) {
    p.setPen(Qt::NoPen); p.setBrush(QColor(33, 46, 57)); p.drawRoundedRect(r, 3, 3);
    p.setBrush(color); QRectF fill = r; fill.setWidth(r.width() * std::clamp(fraction, 0.0, 1.0));
    p.drawRoundedRect(fill, 3, 3);
}
void polygon(QPainter& p, std::initializer_list<QPointF> points, QColor c) {
    QPolygonF poly;
    for (auto pt : points) poly << pt;
    p.setPen(Qt::NoPen); p.setBrush(c); p.drawPolygon(poly);
}
QString elapsed(double seconds) {
    int t = int(seconds); return QString("%1:%2").arg(t / 60, 2, 10, QLatin1Char('0')).arg(t % 60, 2, 10, QLatin1Char('0'));
}
QImage enemySprite(int kind) {
    QImage im(48, 64, QImage::Format_ARGB32_Premultiplied); im.fill(Qt::transparent);
    QPainter p(&im); p.setRenderHint(QPainter::Antialiasing, false);
    QColor c = kind == 0 ? QColor(64, 162, 215) : kind == 1 ? QColor(78, 210, 154) :
               kind == 2 ? QColor(226, 72, 86) : QColor(159, 108, 237);
    QColor dark = c.darker(235), light = c.lighter(145);
    p.fillRect(12, 53, 10, 10, QColor(15, 23, 31)); p.fillRect(28, 53, 10, 10, QColor(15, 23, 31));
    p.fillRect(14, 40, 8, 17, dark); p.fillRect(27, 40, 8, 17, dark);
    p.fillRect(12, 22, 25, 24, dark); p.fillRect(15, 24, 20, 17, c);
    p.fillRect(7, 25, 8, 16, dark); p.fillRect(35, 25, 8, 16, dark);
    p.fillRect(6, 24, 9, 8, c); p.fillRect(35, 24, 9, 8, c);
    p.fillRect(17, 7, 16, 18, dark); p.fillRect(15, 10, 20, 11, c);
    p.fillRect(17, 13, 16, 5, QColor(10, 23, 29)); p.fillRect(18, 14, 14, 2, light);
    p.fillRect(17, 26, 16, 3, light); p.fillRect(18, 33, 14, 4, dark);
    p.fillRect(12, 41, 25, 5, QColor(21, 28, 36)); p.fillRect(23, 42, 5, 3, Amber);
    p.fillRect(6, 35, 7, 11, QColor(83, 104, 115)); p.fillRect(36, 35, 8, 11, QColor(83, 104, 115));
    if (kind == 2) {
        p.fillRect(18, 29, 14, 12, QColor(58, 27, 29));
        p.fillRect(21, 31, 8, 8, QColor(255, 182, 92)); p.fillRect(24, 31, 2, 8, QColor(255, 244, 200));
        p.fillRect(5, 21, 8, 4, Amber); p.fillRect(37, 21, 8, 4, Amber);
    } else if (kind == 3) {
        p.fillRect(3, 23, 11, 11, c); p.fillRect(36, 23, 11, 11, c);
        p.fillRect(19, 3, 3, 8, light); p.fillRect(28, 3, 3, 8, light);
        p.fillRect(20, 31, 10, 8, QColor(238, 199, 255));
        p.fillRect(0, 37, 12, 13, dark); p.fillRect(39, 37, 9, 13, dark);
    } else if (kind == 0) {
        p.fillRect(4, 36, 23, 6, QColor(25, 35, 45)); p.fillRect(3, 35, 8, 3, QColor(111, 131, 141));
    } else {
        p.fillRect(10, 5, 12, 3, light); p.fillRect(27, 5, 12, 3, light);
    }
    return im;
}
QImage objectSprite(int kind) {
    QImage im(32, 40, QImage::Format_ARGB32_Premultiplied); im.fill(Qt::transparent);
    QPainter p(&im);
    if (kind == 3) {
        p.fillRect(6, 5, 21, 33, QColor(106, 63, 38));
        p.fillRect(8, 7, 17, 29, QColor(191, 111, 49));
        p.fillRect(6, 9, 21, 4, QColor(50, 52, 48)); p.fillRect(6, 29, 21, 4, QColor(50, 52, 48));
        polygon(p, {{16,16},{22,25},{10,25}}, Amber); p.fillRect(15, 19, 2, 4, QColor(68,38,27));
    } else {
        QColor color = kind == 0 ? Amber : kind == 1 ? QColor(255, 108, 129) : Teal;
        p.fillRect(4, 17, 24, 20, QColor(18, 35, 43)); p.fillRect(6, 19, 20, 16, color.darker(200));
        p.fillRect(4, 17, 24, 3, color); p.fillRect(4, 34, 24, 3, color);
        if (kind == 1) { p.fillRect(14, 22, 4, 10, color); p.fillRect(11, 25, 10, 4, color); }
        else if (kind == 0) { for (int x = 10; x <= 22; x += 5) p.fillRect(x, 23, 3, 8, color); }
        else polygon(p, {{11,22},{21,22},{20,29},{16,33},{12,29}}, color);
    }
    return im;
}
}

QRectF GameWidget::viewportRect() const {
    const double rail = width() < 1150 ? 232 : 260;
    return QRectF(22, 86, width() - rail - 58, height() - 150);
}

QRectF GameWidget::actionRect(int index) const {
    if (index < 0) return {};
    const double w = std::min(760.0, width() - 96.0);
    QRectF card((width()-w)/2, (height()-540)/2.0+8, w, 540);
    if (m_state == State::Menu) {
        if (index == 0) return QRectF(card.x()+32, card.y()+444, w-64, 56);
        if (index <= 3) return QRectF(card.x()+32+(index-1)*(w-52)/3, card.y()+202, (w-88)/3, 111);
    } else if (m_state == State::Upgrade) {
        if (index <= 2) return QRectF(card.x()+32+index*(w-52)/3, card.y()+221, (w-88)/3, 205);
    } else if (m_state == State::Paused) {
        if (index <= 2) return QRectF(card.x()+100, card.y()+242+index*65, w-200, 50);
    } else if (m_state == State::Won || m_state == State::Lost) {
        if (index <= 1) return QRectF(card.x()+32+index*(w-52)/2, card.y()+444, (w-76)/2, 56);
    }
    return {};
}

void GameWidget::paintEvent(QPaintEvent*) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), Ink);
    const QRectF view = viewportRect();
    drawWorld(p, view);
    drawHud(p, view);
    if (m_state != State::Playing) drawOverlay(p);
}

void GameWidget::drawWorld(QPainter& p, const QRectF& view) {
    p.save(); p.setClipRect(view); p.translate(view.topLeft());
    const double w = view.width(), h = view.height(), horizon = h * 0.48;
    const double fov = Pi * 0.39, plane = w / (2 * std::tan(fov/2));
    const double look = m_angle;
    const double forwardX = std::cos(look), forwardY = std::sin(look);
    const double sideX = -forwardY, sideY = forwardX;
    QLinearGradient sky(0,0,0,horizon); sky.setColorAt(0, QColor(10,18,28)); sky.setColorAt(1,QColor(31,43,52));
    p.fillRect(QRectF(0,0,w,horizon),sky);
    p.fillRect(QRectF(0,horizon,w,h-horizon),QColor(24,32,38));

    // Every ground square is a real map cell, projected from the same 2D coordinates as the walls.
    const int fw = std::max(1,int(w/2)), fh = std::max(1,int((h-horizon)/2));
    QImage floor(fw, fh, QImage::Format_RGB32);
    for (int y=0; y<fh; ++y) {
        QRgb* row = reinterpret_cast<QRgb*>(floor.scanLine(y));
        double distance = plane * 0.5 / std::max(1.0, double(y)*2+1);
        double span = distance * std::tan(fov/2);
        double wx = m_player.x()+forwardX*distance-sideX*span;
        double wy = m_player.y()+forwardY*distance-sideY*span;
        double dx = sideX * span*2/fw, dy = sideY * span*2/fw;
        double fog = std::clamp(1.0-distance/25.0, 0.16, 1.0);
        for (int x=0; x<fw; ++x, wx+=dx,wy+=dy) {
            double fx = wx-std::floor(wx), fy = wy-std::floor(wy);
            bool grid = fx < 0.018 || fy < 0.018 || fx > 0.982 || fy > 0.982;
            bool checker = (int(std::floor(wx))+int(std::floor(wy)))%2 == 0;
            int base = grid ? 69 : checker ? 36 : 32;
            row[x] = qRgb(int(base*fog), int((base+(grid?22:9))*fog), int((base+(grid?27:13))*fog));
        }
    }
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.drawImage(QRectF(0,horizon,w,h-horizon),floor);

    const int columnWidth = 2, columns = int(std::ceil(w/columnWidth));
    std::vector<double> depth(columns,40.0);
    p.setRenderHint(QPainter::Antialiasing, false);
    for(int col=0;col<columns;++col) {
        double screen = (double(col*columnWidth)+1-w/2)/plane;
        double rayAngle = look+std::atan(screen);
        int wall=1,side=0; double u=0;
        double distance=castRay(m_player,rayAngle,40,&side,&wall,&u)*std::cos(rayAngle-look);
        distance=std::max(0.07,distance); depth[col]=distance;
        double wallHeight = plane/distance, top=horizon-wallHeight/2;
        QColor base = wall==2?QColor(50,117,120):wall==3?QColor(137,87,49):QColor(78,95,110);
        double lighting=std::clamp(1.12-distance*0.046,0.18,0.98)*(side?0.76:1.0);
        bool seam=u<0.018||u>0.982; if(seam) lighting*=0.56;
        QColor tint(int(base.red()*lighting),int(base.green()*lighting),int(base.blue()*lighting));
        p.fillRect(QRectF(col*columnWidth,top,columnWidth,wallHeight),tint);
        p.fillRect(QRectF(col*columnWidth,top+wallHeight*0.17,columnWidth,std::max(1.0,wallHeight*.012)),tint.darker(135));
        p.fillRect(QRectF(col*columnWidth,top+wallHeight*0.76,columnWidth,std::max(1.0,wallHeight*.018)),tint.darker(150));
        p.fillRect(QRectF(col*columnWidth,top+wallHeight*0.97,columnWidth,std::max(1.0,wallHeight*.035)),QColor(13,23,29));
        if(wall==2&&u>.15&&u<.85) {
            QColor strip=Teal; strip.setAlpha(int(std::max(40.0,210-distance*7)));
            p.fillRect(QRectF(col*columnWidth,top+wallHeight*.28,columnWidth,std::max(1.0,wallHeight*.025)),strip);
        }
        if(wall==3&&u>.08&&u<.92) {
            QColor stripe = int(u*10)%2 ? Amber.darker(140) : QColor(38,38,31);
            p.fillRect(QRectF(col*columnWidth,top+wallHeight*.66,columnWidth,wallHeight*.06),stripe);
        }
    }

    static const std::array<QImage,4> enemies {{enemySprite(0),enemySprite(1),enemySprite(2),enemySprite(3)}};
    static const std::array<QImage,4> objects {{objectSprite(0),objectSprite(1),objectSprite(2),objectSprite(3)}};
    struct Visible { QPointF pos; int type,index; double distance; };
    std::vector<Visible> visible;
    auto append=[&](QPointF pos,int type,int index){double dx=pos.x()-m_player.x(),dy=pos.y()-m_player.y();visible.push_back({pos,type,index,dx*dx+dy*dy});};
    for(int i=0;i<int(m_enemies.size());++i) if(m_enemies[i].alive)append(m_enemies[i].pos,0,i);
    for(int i=0;i<int(m_pickups.size());++i) if(m_pickups[i].active)append(m_pickups[i].pos,1,i);
    for(int i=0;i<int(m_barrels.size());++i) if(m_barrels[i].active)append(m_barrels[i].pos,2,i);
    for(int i=0;i<int(m_projectiles.size());++i)append(m_projectiles[i].pos,3,i);
    for(int i=0;i<int(m_effects.size());++i)append(m_effects[i].pos,4,i);
    std::sort(visible.begin(),visible.end(),[](const Visible&a,const Visible&b){return a.distance>b.distance;});
    for(const auto& s:visible) {
        double dx=s.pos.x()-m_player.x(),dy=s.pos.y()-m_player.y();
        double distance=dx*forwardX+dy*forwardY;
        if(distance<0.1)continue;
        double screenX=w/2+(dx*sideX+dy*sideY)*plane/distance;
        double unit=plane/distance;
        if(screenX<-unit*2||screenX>w+unit*2)continue;
        if(s.type>=3) {
            int dc=std::clamp(int(screenX/columnWidth),0,columns-1);
            if(distance>depth[dc]+0.2)continue;
            double radius=unit*.07; QColor c(255,108,92); double alpha=1;
            if(s.type==4){const auto&e=m_effects[s.index];alpha=std::clamp(e.life/e.maxLife,0.0,1.0);radius=unit*e.size*(1-alpha*.6);c=e.color;}
            p.setRenderHint(QPainter::Antialiasing,true);
            QRadialGradient glow(QPointF(screenX,horizon),std::max(1.0,radius*2));
            c.setAlpha(int(160*alpha));glow.setColorAt(0,c);c.setAlpha(0);glow.setColorAt(1,c);
            p.setPen(Qt::NoPen);p.setBrush(glow);p.drawEllipse(QPointF(screenX,horizon),radius*2,radius*2);
            c.setAlpha(int(240*alpha));p.setBrush(c);p.drawEllipse(QPointF(screenX,horizon),radius*.55,radius*.55);
            p.setRenderHint(QPainter::Antialiasing,false);continue;
        }
        const QImage* sprite=nullptr; double size=1,vertical=0;
        if(s.type==0){const Enemy&e=m_enemies[s.index];sprite=&enemies[int(e.kind)];size=e.kind==Enemy::Sentinel?1.36:e.kind==Enemy::Runner?.82:1.0;}
        else if(s.type==1){sprite=&objects[int(m_pickups[s.index].kind)];size=.55;vertical=std::sin(m_time*3+s.index)*unit*.018;}
        else{sprite=&objects[3];size=.8;}
        double sh=unit*size,sw=sh*sprite->width()/sprite->height();
        double left=screenX-sw/2,top=horizon+unit*.5-sh+vertical;
        for(int x=std::max(0,int(left));x<std::min(int(w),int(left+sw));x+=2){
            if(distance>depth[std::clamp(x/columnWidth,0,columns-1)]+.08)continue;
            double source=(x-left)/sw*sprite->width();
            p.drawImage(QRectF(x,top,2,sh),*sprite,QRectF(source,0,2/sw*sprite->width(),sprite->height()));
            if(s.type==0&&m_enemies[s.index].hitFlash>0){p.fillRect(QRectF(x,top+sh*.22,2,sh*.55),QColor(255,255,255,65));}
        }
        if(s.type==0&&m_enemies[s.index].health<m_enemies[s.index].maxHealth&&distance<depth[std::clamp(int(screenX/2),0,columns-1)]){
            p.fillRect(QRectF(screenX-22,top-10,44,3),QColor(33,38,47));
            p.fillRect(QRectF(screenX-22,top-10,44*m_enemies[s.index].health/m_enemies[s.index].maxHealth,3),Red);
        }
    }
    p.setRenderHint(QPainter::Antialiasing,true);

    // The foreground viewmodel is drawn with a deliberately crisp, low-poly silhouette.
    p.save();
    double bobX=std::sin(m_bob)*4,bobY=std::abs(std::cos(m_bob))*3;
    double reloadDip=m_reloadTimer>0?40*std::sin(Pi*std::clamp(m_reloadTimer/m_reloadDuration,0.0,1.0)):0;
    p.translate(w*.54+bobX,h+16+bobY+m_recoil*25+reloadDip);
    double weaponScale=std::min(w/760.0,h/560.0); p.scale(weaponScale,weaponScale);
    polygon(p,{{-108,0},{-91,-73},{-52,-93},{-9,-35},{1,0}},QColor(42,59,69));
    polygon(p,{{66,0},{51,-73},{95,-94},{143,-41},{156,0}},QColor(46,62,70));
    polygon(p,{{-78,-56},{-74,-89},{-49,-111},{-19,-89},{-14,-52}},QColor(122,102,86));
    polygon(p,{{59,-57},{48,-99},{79,-121},{105,-90},{110,-59}},QColor(122,102,86));
    if(m_weapon==0){
        polygon(p,{{-31,0},{-54,-86},{-35,-144},{-23,-156},{-18,-226},{16,-226},{22,-156},{42,-128},{58,-24},{43,0}},QColor(25,36,46));
        polygon(p,{{-36,-29},{-36,-111},{-15,-153},{11,-153},{34,-114},{40,-29}},QColor(70,87,99));
        polygon(p,{{-15,-155},{-12,-210},{12,-210},{13,-155}},QColor(103,123,132));
        p.fillRect(QRectF(-12,-226,24,21),QColor(16,27,35));
        p.fillRect(QRectF(-7,-231,14,8),QColor(68,88,97));
        polygon(p,{{-22,-74},{-17,-129},{14,-129},{22,-74}},QColor(23,41,49));
        p.setPen(QPen(Teal,3));p.drawLine(QPointF(-12,-83),QPointF(-9,-119));p.drawLine(QPointF(12,-83),QPointF(9,-119));
        p.setPen(Qt::NoPen);p.setBrush(QColor(9,18,25));p.drawRoundedRect(QRectF(-17,-183,34,26),5,5);
        p.fillRect(QRectF(-9,-176,18,8),QColor(58,139,126));p.fillRect(QRectF(-2,-176,4,8),Teal);
        for(int i=0;i<4;++i)p.fillRect(QRectF(-26,-62+i*10,54,3),QColor(34,49,61));
    } else {
        polygon(p,{{-53,0},{-64,-70},{-42,-154},{-30,-210},{24,-210},{45,-160},{66,-70},{52,0}},QColor(37,40,43));
        p.fillRect(QRectF(-28,-232,25,94),QColor(86,95,97));p.fillRect(QRectF(2,-232,25,94),QColor(59,72,78));
        p.fillRect(QRectF(-28,-231,55,13),QColor(23,29,32));p.fillRect(QRectF(-3,-239,5,14),Amber);
        polygon(p,{{-42,-145},{39,-145},{51,-81},{-51,-81}},QColor(143,85,46));
        for(int i=0;i<5;++i)p.fillRect(QRectF(-40-i,-133+i*10,79+i*2,4),QColor(74,51,36));
        polygon(p,{{-37,-65},{35,-65},{43,-3},{-39,-3}},QColor(78,91,96));
        p.fillRect(QRectF(18,-57,10,34),Amber);
    }
    if(m_muzzleFlash>0){
        double pulse=1+std::sin(m_time*111)*.13;
        p.save();p.translate(0,-234);p.scale(pulse,pulse);
        polygon(p,{{0,-78},{12,-35},{43,-53},{27,-14},{49,2},{14,10},{0,30},{-14,8},{-48,0},{-24,-17},{-42,-54},{-10,-34}},QColor(255,176,68,220));
        polygon(p,{{0,-48},{9,-16},{24,-7},{8,7},{0,19},{-8,3},{-22,-7},{-7,-17}},QColor(255,246,182));p.restore();
    }
    p.restore();

    if(m_state==State::Playing){
        double cx=w/2,cy=horizon,gap=7+m_recoil*7;
        p.setPen(QPen(QColor(232,249,244,220),1.5));
        p.drawLine(QPointF(cx-gap-8,cy),QPointF(cx-gap,cy));p.drawLine(QPointF(cx+gap,cy),QPointF(cx+gap+8,cy));
        p.drawLine(QPointF(cx,cy-gap-8),QPointF(cx,cy-gap));p.drawLine(QPointF(cx,cy+gap),QPointF(cx,cy+gap+8));
        p.setPen(Qt::NoPen);p.setBrush(Teal);p.drawEllipse(QPointF(cx,cy),1.7,1.7);
        if(m_hitMarker>0){p.setPen(QPen(Amber,2));for(int sx:{-1,1})for(int sy:{-1,1})p.drawLine(QPointF(cx+sx*11,cy+sy*11),QPointF(cx+sx*17,cy+sy*17));}
    }
    QRadialGradient vignette(QPointF(w/2,h/2),w*.65);vignette.setColorAt(0,QColor(0,0,0,0));vignette.setColorAt(.55,QColor(0,0,0,0));vignette.setColorAt(1,QColor(2,8,15,140));
    p.fillRect(QRectF(0,0,w,h),vignette);
    if(m_damageFlash>0){
        QRadialGradient damage(QPointF(w/2,h/2),w*.6);damage.setColorAt(0,QColor(230,41,62,0));damage.setColorAt(.35,QColor(230,41,62,0));damage.setColorAt(1,QColor(230,41,62,int(std::min(200.0,m_damageFlash*280))));p.fillRect(QRectF(0,0,w,h),damage);
    }
    box(p,QRectF(16,16,153,30),QColor(10,20,28,195),QColor(56,80,88),4);
    label(p,QRectF(27,16,133,30),"LIVE  /  SECTOR 07",10,Teal,true,Qt::AlignLeft|Qt::AlignVCenter,true);
    QString objective=m_wave>=5?"ELIMINATE THE SENTINEL":"CLEAR THE SECTOR";
    if(m_state==State::Playing){
        label(p,QRectF(18,53,w-36,26),objective,14,White,true);
        label(p,QRectF(18,79,w-36,18),QString("%1 HOSTILES REMAINING").arg(aliveEnemies()),10,Muted,false,Qt::AlignLeft|Qt::AlignVCenter,true);
        if(m_bannerTimer>0){
            QRectF banner(w*.18,h*.19,w*.64,60);box(p,banner,QColor(10,23,31,224),QColor(65,124,125),6);
            label(p,banner.adjusted(12,4,-12,-4),m_banner,19,Teal,true,Qt::AlignCenter);
        }
        if(m_reloadTimer>0){
            label(p,QRectF(w/2-90,horizon+47,180,22),"RELOADING",11,Amber,true,Qt::AlignCenter,true);
            bar(p,QRectF(w/2-65,horizon+76,130,4),1-m_reloadTimer/m_reloadDuration,Amber);
        } else if(m_ammo[m_weapon]==0) label(p,QRectF(w/2-120,horizon+47,240,26),"PRESS R TO RELOAD",12,Red,true,Qt::AlignCenter,true);
    }
    // Subtle pixel scanlines tie the scene and procedural sprites together.
    p.setPen(QColor(0,0,0,15));for(int y=0;y<h;y+=4)p.drawLine(QPointF(0,y),QPointF(w,y));
    p.restore();
    p.setPen(QPen(Edge,1));p.setBrush(Qt::NoBrush);p.drawRect(view);
}

void GameWidget::drawHud(QPainter& p, const QRectF& view) {
    p.setRenderHint(QPainter::Antialiasing,true);
    polygon(p,{{23,28},{38,28},{38,36},{31,36},{31,43},{38,43},{38,52},{23,52}},Teal);
    polygon(p,{{41,28},{49,28},{49,52},{41,52},{41,43},{45,43},{45,36},{41,36}},Teal);
    label(p,QRectF(61,18,255,35),"GRID BREACH",25,White,true);
    label(p,QRectF(62,52,280,16),"A GRID-BASED SURVIVAL FPS",9,Muted,false,Qt::AlignLeft|Qt::AlignVCenter,true);
    label(p,QRectF(width()-325,23,302,23),"RESEARCH FACILITY  /  SECTOR 07",10,Muted,false,Qt::AlignRight|Qt::AlignVCenter,true);
    label(p,QRectF(width()-325,48,302,18),m_sound.muted()?"AUDIO OFF   [M]":"AUDIO ON   [M]",9,m_sound.muted()?Muted:Teal,false,Qt::AlignRight|Qt::AlignVCenter,true);
    double x=view.right()+16,rw=width()-x-22,y=view.y();
    bool compact=height()<780;
    double statusH=compact?135:150;
    box(p,QRectF(x,y,rw,statusH));
    label(p,QRectF(x+16,y+9,rw-32,22),"OPERATOR STATUS",10,Muted,true,Qt::AlignLeft|Qt::AlignVCenter,true);
    label(p,QRectF(x+16,y+35,95,29),QString::number(int(std::ceil(m_health))),27,m_health<30?Red:White,true,Qt::AlignLeft|Qt::AlignVCenter,true);
    label(p,QRectF(x+83,y+42,rw-100,20),QString("/ %1 HP").arg(int(m_maxHealth)),11,Muted,false,Qt::AlignLeft|Qt::AlignVCenter,true);
    bar(p,QRectF(x+16,y+72,rw-32,7),m_health/m_maxHealth,m_health<30?Red:Teal);
    label(p,QRectF(x+16,y+87,rw-32,18),QString("SHIELD                           %1").arg(int(m_shield)),9,Muted,false,Qt::AlignLeft|Qt::AlignVCenter,true);
    bar(p,QRectF(x+16,y+111,rw-32,4),m_shield/100,QColor(90,157,249));
    if(!compact)bar(p,QRectF(x+16,y+130,rw-32,3),m_stamina/100,Amber);
    y+=statusH+12;
    double mapH=compact?174:214;
    box(p,QRectF(x,y,rw,mapH));
    label(p,QRectF(x+16,y+10,rw-32,20),"TACTICAL GRID",10,Muted,true,Qt::AlignLeft|Qt::AlignVCenter,true);
    label(p,QRectF(x+16,y+10,rw-32,20),"[TAB]",9,Muted,false,Qt::AlignRight|Qt::AlignVCenter,true);
    if(m_showMap){
        double size=std::min(rw-32,mapH-47),cell=size/MapSize;
        QRectF grid(x+(rw-size)/2,y+36,size,size);
        p.fillRect(grid,QColor(8,17,23));
        for(int row=0;row<MapSize;++row)for(int col=0;col<MapSize;++col){
            int v=tile(col,row);QColor color=v==0?QColor(23,38,46):v==2?QColor(58,108,107):v==3?QColor(117,87,52):QColor(64,80,91);
            p.fillRect(QRectF(grid.x()+col*cell,grid.y()+row*cell,cell-.5,cell-.5),color);
        }
        auto mapPoint=[&](QPointF pt){return grid.topLeft()+QPointF(pt.x()*cell,pt.y()*cell);};
        p.setPen(Qt::NoPen);
        for(const auto& pickup:m_pickups)if(pickup.active){p.setBrush(pickup.kind==Pickup::Ammo?Amber:pickup.kind==Pickup::Health?QColor(255,132,148):Teal);p.drawRect(QRectF(mapPoint(pickup.pos)-QPointF(1.5,1.5),QSizeF(3,3)));}
        for(const auto&e:m_enemies)if(e.alive){p.setBrush(e.kind==Enemy::Sentinel?QColor(192,147,255):Red);p.drawEllipse(mapPoint(e.pos),e.kind==Enemy::Sentinel?3.5:2,e.kind==Enemy::Sentinel?3.5:2);}
        QPointF player=mapPoint(m_player);p.setBrush(QColor(91,230,202,38));p.setPen(QPen(QColor(91,230,202,70),1));
        QPolygonF cone;cone<<player<<player+QPointF(std::cos(m_angle-.45),std::sin(m_angle-.45))*cell*3<<player+QPointF(std::cos(m_angle+.45),std::sin(m_angle+.45))*cell*3;p.drawPolygon(cone);
        p.setPen(QPen(Teal,1.5));p.drawLine(player,player+QPointF(std::cos(m_angle),std::sin(m_angle))*cell*1.5);
        p.setBrush(White);p.setPen(Qt::NoPen);p.drawEllipse(player,3.2,3.2);
    }else label(p,QRectF(x+16,y+45,rw-32,mapH-60),"MAP HIDDEN\nPress Tab to reveal",12,Muted,false,Qt::AlignCenter);
    y+=mapH+12;
    double waveH=compact?74:88;
    box(p,QRectF(x,y,rw,waveH));
    label(p,QRectF(x+16,y+8,rw-32,20),"WAVE",10,Muted,true,Qt::AlignLeft|Qt::AlignVCenter,true);
    label(p,QRectF(x+16,y+29,95,38),QString("%1 / 5").arg(std::max(1,m_wave)),25,White,true,Qt::AlignLeft|Qt::AlignVCenter,true);
    label(p,QRectF(x+115,y+10,rw-131,20),"SCORE",9,Muted,false,Qt::AlignRight|Qt::AlignVCenter,true);
    label(p,QRectF(x+99,y+32,rw-115,30),QString::number(m_score),22,Amber,true,Qt::AlignRight|Qt::AlignVCenter,true);
    y+=waveH+12;
    double ammoH=view.bottom()-y;
    box(p,QRectF(x,y,rw,ammoH));
    label(p,QRectF(x+16,y+8,rw-32,20),m_weapon==0?"01 / PULSE RIFLE":"02 / BREACH SHOTGUN",10,Muted,true,Qt::AlignLeft|Qt::AlignVCenter,true);
    label(p,QRectF(x+16,y+33,83,41),QString::number(m_ammo[m_weapon]).rightJustified(2,'0'),36,m_ammo[m_weapon]?White:Red,true,Qt::AlignLeft|Qt::AlignVCenter,true);
    label(p,QRectF(x+93,y+42,rw-109,27),QString("/ %1").arg(m_reserve[m_weapon]),18,Muted,false,Qt::AlignLeft|Qt::AlignVCenter,true);
    if(ammoH>100)label(p,QRectF(x+16,y+84,rw-32,19),"R  RELOAD     1 / 2  SWITCH",9,Muted,false,Qt::AlignLeft|Qt::AlignVCenter,true);
    p.setPen(QPen(Edge,1));p.drawLine(QPointF(22,height()-47),QPointF(width()-22,height()-47));
    label(p,QRectF(23,height()-39,width()-46,27),"WASD  MOVE    MOUSE  AIM    CLICK  FIRE    SHIFT  SPRINT    SPACE  DASH",10,Muted,false,Qt::AlignLeft|Qt::AlignVCenter,true);
    label(p,QRectF(23,height()-39,width()-46,27),"ESC  PAUSE",10,Teal,false,Qt::AlignRight|Qt::AlignVCenter,true);
}

void GameWidget::drawOverlay(QPainter& p) {
    p.fillRect(rect(),QColor(4,10,17,192));
    const double w=std::min(760.0,width()-96.0);
    QRectF card((width()-w)/2,(height()-540)/2.0+8,w,540);
    // A thin signal strip and restrained typography keep the menu distinct from the HUD.
    box(p,card,QColor(15,25,34),QColor(59,84,96),14);
    p.setPen(Qt::NoPen);p.setBrush(Teal);p.drawRoundedRect(QRectF(card.x()+32,card.y()+28,35,4),2,2);
    const QPoint hover=mapFromGlobal(QCursor::pos());
    auto button=[&](int i,const QString& title,bool primary){
        QRectF r=actionRect(i);bool hot=r.contains(hover);
        box(p,r,primary?(hot?Teal.lighter(110):Teal):(hot?QColor(37,59,70):QColor(25,39,50)),primary?Teal:Edge,6);
        label(p,r.adjusted(12,0,-12,0),title,14,primary?QColor(13,35,38):White,true,Qt::AlignCenter);
    };
    QString difficulty=QStringList{"EASY","STANDARD","HARD"}.at(m_difficulty);
    if(m_state==State::Menu){
        label(p,QRectF(card.x()+32,card.y()+47,w-64,24),"OPERATION / GRID BREACH",11,Teal,true,Qt::AlignLeft|Qt::AlignVCenter,true);
        label(p,QRectF(card.x()+30,card.y()+76,w-60,58),"Breach. Survive. Extract.",36,White,true);
        label(p,QRectF(card.x()+32,card.y()+137,w-64,44),"Five waves. Two weapons. One way out.\nNavigate the grid, manage your ammo, and take down the Sentinel.",14,Muted,false,Qt::AlignLeft|Qt::AlignVCenter);
        const QStringList names{"EASY","STANDARD","HARD"};
        const QStringList subs{"Room to find your aim","The intended challenge","No room for hesitation"};
        for(int i=0;i<3;++i){
            QRectF r=actionRect(i+1);bool selected=i==m_difficulty;
            box(p,r,selected?QColor(25,52,56):r.contains(hover)?QColor(31,44,55):QColor(20,32,42),selected?Teal:Edge,8);
            label(p,r.adjusted(16,10,-16,-70),names[i],13,selected?Teal:White,true);
            label(p,QRectF(r.x()+16,r.y()+45,r.width()-32,20),subs[i],11,Muted);
            label(p,QRectF(r.x()+16,r.y()+76,r.width()-32,20),QString("BEST  %1").arg(m_best[i]),10,selected?Teal:Muted,false,Qt::AlignLeft|Qt::AlignVCenter,true);
            if(selected){p.setPen(Qt::NoPen);p.setBrush(Teal);p.drawEllipse(QPointF(r.right()-19,r.y()+25),3,3);}
        }
        label(p,QRectF(card.x()+32,card.y()+337,w-64,21),"YOUR FIELD MANUAL",10,Amber,true,Qt::AlignLeft|Qt::AlignVCenter,true);
        label(p,QRectF(card.x()+32,card.y()+370,w-64,48),"WASD to move · Mouse to aim · Click to fire · R to reload\n1 / 2 switch weapons · Shift to sprint · Space to dash · Esc to pause",12,Muted,false,Qt::AlignLeft|Qt::AlignVCenter);
        button(0,"DEPLOY INTO SECTOR 07   →",true);
        label(p,QRectF(card.x()+32,card.y()+505,w-64,21),"2D GRID WORLD  /  FIRST-PERSON RAYCASTING",9,Muted,false,Qt::AlignCenter,true);
    }else if(m_state==State::Paused){
        label(p,QRectF(card.x()+32,card.y()+53,w-64,30),"SIGNAL ON HOLD",11,Teal,true,Qt::AlignCenter,true);
        label(p,QRectF(card.x()+32,card.y()+101,w-64,55),"Take a breath.",36,White,true,Qt::AlignCenter);
        label(p,QRectF(card.x()+32,card.y()+171,w-64,39),QString("Wave %1 of 5   ·   %2 points   ·   %3 elapsed").arg(m_wave).arg(m_score).arg(elapsed(m_time)),14,Muted,false,Qt::AlignCenter);
        button(0,"RESUME OPERATION",true);button(1,"RESTART RUN",false);button(2,"BACK TO MENU",false);
        label(p,QRectF(card.x()+32,card.y()+469,w-64,26),"Press Esc to resume. Your run is paused.",12,Muted,false,Qt::AlignCenter);
    }else if(m_state==State::Upgrade){
        label(p,QRectF(card.x()+32,card.y()+50,w-64,27),QString("WAVE %1 CLEARED").arg(m_wave),11,Teal,true,Qt::AlignLeft|Qt::AlignVCenter,true);
        label(p,QRectF(card.x()+32,card.y()+91,w-64,55),"Adapt. Then advance.",33,White,true);
        label(p,QRectF(card.x()+32,card.y()+159,w-64,30),"Choose one permanent upgrade for the next wave.",14,Muted);
        const QStringList titles{"OVERCHARGE","REINFORCE","FAST HANDS"};
        const QStringList values{"+20%","+25","+20%"};
        const QStringList desc{"Weapon damage\nEvery shot hits harder.","Maximum health\nRestore your health, too.","Reload speed\nGet back into the fight."};
        for(int i=0;i<3;++i){QRectF r=actionRect(i);bool hot=r.contains(hover);box(p,r,hot?QColor(25,54,58):QColor(21,36,47),hot?Teal:Edge,9);
            label(p,QRectF(r.x()+17,r.y()+17,r.width()-34,23),QString("0%1 / UPGRADE").arg(i+1),9,Muted,false,Qt::AlignLeft|Qt::AlignVCenter,true);
            label(p,QRectF(r.x()+17,r.y()+51,r.width()-34,45),values[i],36,i==1?Teal:Amber,true,Qt::AlignLeft|Qt::AlignVCenter,true);
            label(p,QRectF(r.x()+17,r.y()+109,r.width()-34,25),titles[i],12,White,true);
            label(p,QRectF(r.x()+17,r.y()+143,r.width()-34,46),desc[i],11,Muted);}
        label(p,QRectF(card.x()+32,card.y()+462,w-64,38),"The next wave starts after you choose. Check the map for supplies.",12,Muted,false,Qt::AlignCenter);
    }else{
        bool won=m_state==State::Won;QColor accent=won?Teal:Red;
        label(p,QRectF(card.x()+32,card.y()+48,w-64,27),won?"OPERATION COMPLETE":"OPERATOR DOWN",11,accent,true,Qt::AlignLeft|Qt::AlignVCenter,true);
        label(p,QRectF(card.x()+32,card.y()+85,w-64,53),won?"Sector secured.":"The grid fights back.",35,White,true);
        label(p,QRectF(card.x()+32,card.y()+145,w-64,24),won?"The Sentinel is offline. You made it out.":"Recover your aim, adjust your route, and try again.",14,Muted);
        QRectF score(card.x()+32,card.y()+193,w-64,98);box(p,score,QColor(21,39,48),Edge,8);
        label(p,score.adjusted(21,10,-21,-58),"FINAL SCORE",10,Muted,true,Qt::AlignLeft|Qt::AlignVCenter,true);
        label(p,QRectF(score.x()+21,score.y()+37,score.width()-42,45),QString::number(m_score),34,Amber,true,Qt::AlignLeft|Qt::AlignVCenter,true);
        label(p,QRectF(score.x()+200,score.y()+18,score.width()-222,25),m_newBest?"NEW PERSONAL BEST":QString("BEST  %1").arg(m_best[m_difficulty]),11,m_newBest?Teal:Muted,true,Qt::AlignRight|Qt::AlignVCenter,true);
        label(p,QRectF(score.x()+200,score.y()+55,score.width()-222,24),difficulty,11,Muted,false,Qt::AlignRight|Qt::AlignVCenter,true);
        QStringList values{QString::number(m_kills),QString("%1%").arg(m_shots?std::min(100,int(m_hits*100.0/m_shots)):0),elapsed(m_time),QString("%1 / 5").arg(m_wave)};
        QStringList titles{"ELIMINATIONS","ACCURACY","TIME SURVIVED","WAVES REACHED"};
        for(int i=0;i<4;++i){double cx=card.x()+32+i*(w-64)/4;double cw=(w-64)/4;
            label(p,QRectF(cx,card.y()+318,cw,34),values[i],23,White,true,Qt::AlignCenter,true);
            label(p,QRectF(cx,card.y()+360,cw,20),titles[i],8,Muted,true,Qt::AlignCenter,true);}
        label(p,QRectF(card.x()+32,card.y()+398,w-64,23),won?"Keep the streak alive. Try the next difficulty.":"Tip: red barrels clear groups. Keep your distance from the blast.",11,Muted,false,Qt::AlignCenter);
        button(0,"DEPLOY AGAIN",true);button(1,"BACK TO MENU",false);
    }
}
