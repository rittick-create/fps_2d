#include "netmatch.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QHostAddress>
#include <algorithm>
#include <cmath>
namespace {
constexpr double Pi=3.14159265358979323846;
const std::array<int,2> Capacity {{18,6}};
double length(QPointF p) {return std::hypot(p.x(),p.y());}
QPointF direction(double angle) {return {std::cos(angle),std::sin(angle)};}
double dot(QPointF a,QPointF b) {return a.x()*b.x()+a.y()*b.y();}
QJsonArray ammoArray(const std::array<int,2>& values) {return {values[0],values[1]};}
QJsonObject serialize(const NetPlayer& p) {
    return {{"id",p.id},{"team",p.team},{"name",p.name},{"connected",p.connected},
        {"x",p.pos.x()},{"y",p.pos.y()},{"angle",p.angle},{"health",p.health},{"shield",p.shield},{"stamina",p.stamina},
        {"kills",p.kills},{"deaths",p.deaths},{"weapon",p.weapon},{"shots",p.shots},{"hits",p.hits},
        {"ammo",ammoArray(p.ammo)},{"reserve",ammoArray(p.reserve)},{"reload",p.reload},
        {"respawn",p.respawn},{"protection",p.protection},{"dashCooldown",p.dashCooldown}};
}
NetPlayer deserialize(const QJsonObject& o) {
    NetPlayer p;p.id=o["id"].toInt(-1);p.team=o["team"].toInt();p.name=o["name"].toString();p.connected=o["connected"].toBool();
    p.pos={o["x"].toDouble(),o["y"].toDouble()};p.angle=o["angle"].toDouble();
    p.health=o["health"].toDouble();p.shield=o["shield"].toDouble();p.stamina=o["stamina"].toDouble();
    p.kills=o["kills"].toInt();p.deaths=o["deaths"].toInt();p.weapon=o["weapon"].toInt();
    p.shots=o["shots"].toInt();p.hits=o["hits"].toInt();p.reload=o["reload"].toDouble();
    p.respawn=o["respawn"].toDouble();p.protection=o["protection"].toDouble();p.dashCooldown=o["dashCooldown"].toDouble();
    for(int i=0;i<2;++i){p.ammo[i]=o["ammo"].toArray().at(i).toInt();p.reserve[i]=o["reserve"].toArray().at(i).toInt();}
    return p;
}
}
NetMatch::NetMatch(QObject* parent):QObject(parent) {
    connect(&m_server,&QTcpServer::newConnection,this,&NetMatch::acceptConnections);
    connect(&m_client,&QTcpSocket::connected,this,[this]{
        write(&m_client,{{"type","hello"},{"version",1},{"name",m_name}});m_status="Connected. Waiting for the lobby.";emit changed();
    });
    connect(&m_client,&QTcpSocket::readyRead,this,&NetMatch::receiveClient);
    connect(&m_client,&QTcpSocket::disconnected,this,[this]{
        if(m_leaving)return;m_localId=-1;m_snapshot.started=false;m_status="Disconnected from host. Return to the lobby to reconnect.";emit changed();
    });
    connect(&m_client,&QTcpSocket::errorOccurred,this,[this](QAbstractSocket::SocketError){
        if(m_leaving)return;m_status="Connection error: "+m_client.errorString();emit changed();
    });
    m_clock.start();m_last=m_clock.elapsed();
    m_timer.setTimerType(Qt::PreciseTimer);m_timer.setInterval(16);
    connect(&m_timer,&QTimer::timeout,this,&NetMatch::updateClock);m_timer.start();
}
NetMatch::~NetMatch(){leave();}
void NetMatch::write(QTcpSocket* socket,const QJsonObject& object) {
    if(!socket || socket->state()!=QAbstractSocket::ConnectedState)return;
    if(socket->bytesToWrite()>262144){socket->abort();return;}
    socket->write(QJsonDocument(object).toJson(QJsonDocument::Compact)+'\n');
}
void NetMatch::leave() {
    m_leaving=true;m_client.abort();m_server.close();
    for(auto& socket:m_peers) if(socket){socket->disconnect(this);socket->abort();socket->deleteLater();socket=nullptr;}
    for(auto& buffer:m_peerBuffers)buffer.clear();
    m_inputs={};m_authority={};m_snapshot={};m_clientBuffer.clear();m_localId=-1;
    m_status="Host a match or join a host on your Wi-Fi.";m_leaving=false;
}
bool NetMatch::host(const QString& name,const ArenaGrid& grid,quint16 port) {
    leave();m_grid=grid;m_name=name.trimmed().left(20);if(m_name.isEmpty())m_name="Host";
    if(!m_server.listen(QHostAddress::AnyIPv4,port)){m_status="Cannot host: "+m_server.errorString();emit changed();return false;}
    m_status="Hosting on port "+QString::number(m_server.serverPort());m_client.connectToHost(QHostAddress::LocalHost,m_server.serverPort());emit changed();return true;
}
void NetMatch::join(const QString& name,const QString& address,quint16 port) {
    leave();m_name=name.trimmed().left(20);if(m_name.isEmpty())m_name="Player";
    m_status="Connecting to "+address.trimmed()+"…";m_client.connectToHost(address.trimmed(),port);emit changed();
}
void NetMatch::acceptConnections() {
    while(m_server.hasPendingConnections()) {
        QTcpSocket* socket=m_server.nextPendingConnection();socket->setSocketOption(QAbstractSocket::LowDelayOption,1);
        int id=0;while(id<4 && m_peers[id])++id;
        if(id==4 || (m_authority.started && !m_authority.finished)) {write(socket,{{"type","error"},{"message","The lobby is full or a match is already running."}});socket->disconnectFromHost();connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);continue;}
        m_peers[id]=socket;m_peerBuffers[id].clear();m_inputs[id]={};m_inputTimes[id]=m_clock.elapsed();
        m_authority.players[id]={};m_authority.players[id].id=id;m_authority.players[id].team=id%2;
        connect(socket,&QTcpSocket::readyRead,this,[this,id]{receivePeer(id);});
        connect(socket,&QTcpSocket::disconnected,this,[this,id,socket]{onPeerLost(id,socket);});
        write(socket,{{"type","welcome"},{"id",id}});
    }
}
void NetMatch::onPeerLost(int id,QTcpSocket* socket) {
    if(m_peers[id]!=socket)return;
    m_peers[id]=nullptr;socket->deleteLater();m_authority.players[id].connected=false;m_inputs[id]={};
    if(m_authority.started){m_authority.started=false;m_authority.finished=false;m_authority.notice="A player disconnected. Match cancelled; waiting for four players.";}
    publish();
}
void NetMatch::receivePeer(int id) {
    QTcpSocket* socket=m_peers[id];if(!socket)return;
    QByteArray& buffer=m_peerBuffers[id];buffer+=socket->readAll();
    if(buffer.size()>16384){socket->abort();return;}
    while(buffer.contains('\n')) {
        int end=buffer.indexOf('\n');QJsonParseError error;
        auto document=QJsonDocument::fromJson(buffer.left(end),&error);buffer.remove(0,end+1);
        if(error.error!=QJsonParseError::NoError || !document.isObject()){socket->abort();return;}
        QJsonObject o=document.object();QString type=o["type"].toString();
        if(type=="hello") {
            if(o["version"].toInt()!=1){write(socket,{{"type","error"},{"message","Game versions do not match."}});socket->disconnectFromHost();return;}
            auto& p=m_authority.players[id];p.name=o["name"].toString().trimmed().left(20);if(p.name.isEmpty())p.name="Player "+QString::number(id+1);
            p.connected=true;m_authority.notice="Four humans required. Teams are assigned alternately as players join.";publish();
        } else if(type=="input" && m_authority.players[id].connected) {
            double angle=o["angle"].toDouble();if(!std::isfinite(angle)){socket->abort();return;}
            auto& input=m_inputs[id];input.angle=std::remainder(angle,2*Pi);input.movement=o["movement"].toInt()&31;
            input.weapon=std::clamp(o["weapon"].toInt(),0,1);input.fire=o["fire"].toBool();
            input.reload=input.reload||o["reload"].toBool();input.dash=input.dash||o["dash"].toBool();m_inputTimes[id]=m_clock.elapsed();
        }
    }
}
void NetMatch::receiveClient() {
    m_clientBuffer+=m_client.readAll();if(m_clientBuffer.size()>262144){m_client.abort();return;}
    while(m_clientBuffer.contains('\n')) {
        int end=m_clientBuffer.indexOf('\n');auto document=QJsonDocument::fromJson(m_clientBuffer.left(end));m_clientBuffer.remove(0,end+1);
        if(document.isObject())parseClient(document.object());
    }
}
void NetMatch::parseClient(const QJsonObject& o) {
    QString type=o["type"].toString();
    if(type=="welcome") {m_localId=o["id"].toInt(-1);if(m_localId<0 || m_localId>=4)m_client.abort();}
    else if(type=="error")m_status=o["message"].toString();
    else if(type=="snapshot") {
        auto players=o["players"].toArray();if(players.size()!=4)return;
        for(int i=0;i<4;++i)m_snapshot.players[i]=deserialize(players[i].toObject());
        m_snapshot.started=o["started"].toBool();m_snapshot.finished=o["finished"].toBool();
        m_snapshot.remaining=o["remaining"].toDouble();m_snapshot.notice=o["notice"].toString();
        auto scores=o["scores"].toArray();m_snapshot.scores={{scores.at(0).toInt(),scores.at(1).toInt()}};
        m_status=m_snapshot.notice;
    }
    emit changed();
}
void NetMatch::sendInput(const NetInput& input) {
    write(&m_client,{{"type","input"},{"angle",input.angle},{"movement",input.movement},{"weapon",input.weapon},
        {"fire",input.fire},{"reload",input.reload},{"dash",input.dash}});
}
bool NetMatch::startMatch() {
    if(!isHost() || (m_authority.started && !m_authority.finished))return false;
    for(const auto& p:m_authority.players)if(!p.connected)return false;
    m_authority.started=true;m_authority.finished=false;m_authority.remaining=MatchSeconds;m_authority.scores={{0,0}};
    m_authority.notice="10-minute team deathmatch. Blue vs Red. Friendly fire is off.";
    for(int i=0;i<4;++i) {m_authority.players[i].kills=m_authority.players[i].deaths=m_authority.players[i].shots=m_authority.players[i].hits=0;m_inputs[i]={};respawn(i);}
    m_last=m_clock.elapsed();publish();return true;
}
bool NetMatch::walkable(QPointF p) const {
    for(int y=int(std::floor(p.y()-.22));y<=int(std::floor(p.y()+.22));++y)
        for(int x=int(std::floor(p.x()-.22));x<=int(std::floor(p.x()+.22));++x)
            if(x<0 || y<0 || x>=24 || y>=24 || m_grid[y][x])return false;
    return true;
}
void NetMatch::move(QPointF& p,QPointF delta) {
    int count=std::max(1,int(std::ceil(length(delta)/.1)));delta/=count;
    for(int i=0;i<count;++i){QPointF next=p+QPointF(delta.x(),0);if(walkable(next))p=next;next=p+QPointF(0,delta.y());if(walkable(next))p=next;}
}
double NetMatch::ray(QPointF pos,double angle,double limit) const {
    QPointF d=direction(angle);int x=int(std::floor(pos.x())),y=int(std::floor(pos.y()));
    auto wall=[this](int x,int y){return x<0||y<0||x>=24||y>=24||m_grid[y][x]!=0;};
    double dx=std::abs(d.x())<1e-10?1e30:std::abs(1/d.x()),dy=std::abs(d.y())<1e-10?1e30:std::abs(1/d.y());
    int sx=d.x()<0?-1:1,sy=d.y()<0?-1:1;double tx=(sx>0?x+1-pos.x():pos.x()-x)*dx,ty=(sy>0?y+1-pos.y():pos.y()-y)*dy;
    double distance=0;
    for(int i=0;i<96 && !wall(x,y);++i){if(tx<ty){distance=tx;tx+=dx;x+=sx;}else{distance=ty;ty+=dy;y+=sy;}if(distance>=limit)return limit;}
    return std::min(limit,distance);
}
void NetMatch::respawn(int id) {
    auto& p=m_authority.players[id];
    const std::array<QPointF,4> blue {{{3.5,3.5},{3.5,6.5},{5.5,19.5},{11.5,11.5}}};
    const std::array<QPointF,4> red {{{20.5,20.5},{20.5,17.5},{20.5,6.5},{13.5,13.5}}};
    const auto& spots=p.team==0?blue:red;double best=-1;QPointF selected=spots[(id/2)%2];
    for(int n=0;n<4;++n){auto spot=spots[(n+id/2)%4];if(!walkable(spot))continue;double safety=100;
        for(const auto& other:m_authority.players)if(other.connected && other.id!=id && other.health>0)safety=std::min(safety,length(spot-other.pos));
        if(safety>best){best=safety;selected=spot;}}
    p.pos=selected;p.angle=p.team==0?0:Pi;p.health=100;p.shield=25;p.stamina=100;p.ammo={{18,6}};p.reserve={{90,24}};
    p.weapon=0;p.reload=p.cooldown=p.respawn=p.dashCooldown=0;p.protection=2;m_inputs[id].angle=p.angle;m_inputs[id].weapon=0;
}
void NetMatch::damage(int victim,int attacker,double amount) {
    auto& p=m_authority.players[victim];auto& shooter=m_authority.players[attacker];
    if(p.team==shooter.team || p.health<=0 || p.protection>0)return;
    double absorbed=std::min(p.shield,amount*.65);p.shield-=absorbed;p.health=std::max(0.0,p.health-amount+absorbed);
    if(p.health==0){++p.deaths;++shooter.kills;++m_authority.scores[shooter.team];p.respawn=3;p.reload=0;m_inputs[victim]={};
        m_authority.notice=shooter.name+" eliminated "+p.name;}
}
void NetMatch::fire(int id) {
    auto& p=m_authority.players[id];if(p.cooldown>0 || p.reload>0 || p.health<=0)return;
    if(p.ammo[p.weapon]==0){if(p.reserve[p.weapon]>0)p.reload=p.weapon==0?1.35:1.8;return;}
    --p.ammo[p.weapon];++p.shots;p.cooldown=p.weapon==0?.19:.72;p.protection=0;bool hit=false;
    for(int pellet=0;pellet<(p.weapon==0?1:7);++pellet){double angle=p.angle+(p.weapon==0?0:(pellet-3)*.024);QPointF d=direction(angle);double nearest=ray(p.pos,angle,24);int victim=-1;
        for(const auto& target:m_authority.players)if(target.connected && target.id!=id && target.health>0){
            QPointF delta=target.pos-p.pos;double along=dot(delta,d),perp=dot(delta,delta)-along*along;if(along<=0||perp>.09)continue;
            double distance=std::max(0.0,along-std::sqrt(std::max(0.0,.09-perp)));if(distance<nearest){nearest=distance;victim=target.id;}}
        if(victim>=0 && m_authority.players[victim].team!=p.team && m_authority.players[victim].protection<=0){damage(victim,id,p.weapon==0?34:15);hit=true;}
    }
    if(hit)++p.hits;
}
void NetMatch::simulate(double dt) {
    if(!m_authority.started || m_authority.finished)return;
    m_authority.remaining=std::max(0.0,m_authority.remaining-dt);
    if(m_authority.remaining==0){m_authority.finished=true;m_authority.notice=m_authority.scores[0]==m_authority.scores[1]?"Match drawn":m_authority.scores[0]>m_authority.scores[1]?"Blue team wins":"Red team wins";publish();return;}
    for(int id=0;id<4;++id){auto& p=m_authority.players[id];auto& input=m_inputs[id];if(!p.connected)continue;
        if(m_clock.elapsed()-m_inputTimes[id]>500){input.movement=0;input.fire=false;}
        if(p.health<=0){p.respawn=std::max(0.0,p.respawn-dt);if(p.respawn==0)respawn(id);continue;}
        p.angle=input.angle;p.protection=std::max(0.0,p.protection-dt);p.cooldown=std::max(0.0,p.cooldown-dt);p.dashCooldown=std::max(0.0,p.dashCooldown-dt);
        if(p.weapon!=input.weapon){p.weapon=input.weapon;p.reload=0;p.cooldown=.22;}
        if(p.reload>0){p.reload=std::max(0.0,p.reload-dt);if(p.reload==0){int count=std::min(Capacity[p.weapon]-p.ammo[p.weapon],p.reserve[p.weapon]);p.ammo[p.weapon]+=count;p.reserve[p.weapon]-=count;}}
        if(input.reload && p.reload==0 && p.ammo[p.weapon]<Capacity[p.weapon] && p.reserve[p.weapon]>0)p.reload=p.weapon==0?1.35:1.8;
        input.reload=false;
        QPointF velocity;if(input.movement&1)velocity+=direction(p.angle);if(input.movement&2)velocity-=direction(p.angle);if(input.movement&4)velocity+=direction(p.angle-Pi/2);if(input.movement&8)velocity+=direction(p.angle+Pi/2);
        bool moving=length(velocity)>.1,sprint=moving&&(input.movement&16)&&p.stamina>2;
        if(moving)move(p.pos,velocity/length(velocity)*(sprint?4.2:2.8)*dt);
        p.stamina=std::clamp(p.stamina+dt*(sprint?-28:20),0.0,100.0);
        if(input.dash && p.dashCooldown==0 && p.stamina>=25){QPointF dash=moving?velocity/length(velocity):direction(p.angle);move(p.pos,dash*1.7);p.dashCooldown=2.5;p.stamina-=25;}input.dash=false;
        if(input.fire)fire(id);
        // Supply pads refill ammo, health, and shields when occupied, with per-player cooldown.
        for(QPointF pad:{QPointF(2.5,6.5),QPointF(20.5,21.5),QPointF(12.5,12.5)})if(length(p.pos-pad)<.55){
            p.reserve[0]=std::min(180,p.reserve[0]+1);p.reserve[1]=std::min(48,p.reserve[1]+1);p.health=std::min(100.0,p.health+dt*10);p.shield=std::min(25.0,p.shield+dt*5);break;}
    }
}
void NetMatch::publish() {
    QJsonArray players;for(const auto& p:m_authority.players)players.append(serialize(p));
    QJsonObject o{{"type","snapshot"},{"players",players},{"scores",ammoArray(m_authority.scores)},{"started",m_authority.started},
        {"finished",m_authority.finished},{"remaining",m_authority.remaining},{"notice",m_authority.notice}};
    for(auto socket:m_peers)write(socket,o);
}
void NetMatch::updateClock() {
    qint64 now=m_clock.elapsed();double elapsed=std::max(0.0,(now-m_last)/1000.0);m_last=now;
    if(!isHost())return;
    // Match time is real elapsed time; movement never catches up in a large jump.
    double dt=std::min(elapsed,.05);
    if(m_authority.started&&!m_authority.finished && elapsed>dt)m_authority.remaining=std::max(0.0,m_authority.remaining-(elapsed-dt));
    simulate(dt);m_sendAccumulator+=elapsed;
    if(m_sendAccumulator>=.05){m_sendAccumulator=0;publish();}
    for(int id=0;id<4;++id)if(m_peers[id] && !m_authority.players[id].connected && now-m_inputTimes[id]>5000)m_peers[id]->abort();
}
