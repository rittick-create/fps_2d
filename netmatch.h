#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QPointF>
#include <array>

using ArenaGrid = std::array<std::array<int,24>,24>;
struct NetInput {
    double angle = 0;
    int movement = 0; // W=1 S=2 A=4 D=8, sprint=16
    bool fire = false, reload = false, dash = false;
    int weapon = 0;
};
struct NetPlayer {
    int id = -1, team = 0;
    QString name;
    bool connected = false;
    QPointF pos;
    double angle = 0, health = 100, shield = 25, stamina = 100;
    int kills = 0, deaths = 0, weapon = 0, shots = 0, hits = 0;
    std::array<int,2> ammo {{18,6}}, reserve {{90,24}};
    double reload = 0, cooldown = 0, respawn = 0, protection = 0, dashCooldown = 0;
};
struct NetSnapshot {
    std::array<NetPlayer,4> players;
    std::array<int,2> scores {{0,0}};
    bool started = false, finished = false;
    double remaining = 600;
    QString notice;
};

// The host runs the only simulation. Clients send controls, never position or damage.
class NetMatch : public QObject {
    Q_OBJECT
public:
    explicit NetMatch(QObject* parent = nullptr);
    ~NetMatch() override;
    bool host(const QString& name, const ArenaGrid& grid, quint16 port = 45454);
    void join(const QString& name, const QString& address, quint16 port = 45454);
    void leave();
    bool startMatch();
    void sendInput(const NetInput& input);
    bool isHost() const { return m_server.isListening(); }
    bool connected() const { return m_client.state()==QAbstractSocket::ConnectedState; }
    int localId() const { return m_localId; }
    quint16 port() const { return m_server.serverPort(); }
    QString status() const { return m_status; }
    const NetSnapshot& snapshot() const { return m_snapshot; }
    static constexpr double MatchSeconds = 600;
signals:
    void changed();
private:
    void acceptConnections();
    void receivePeer(int id);
    void receiveClient();
    void onPeerLost(int id, QTcpSocket* socket);
    void simulate(double dt);
    void publish();
    void respawn(int id);
    void damage(int victim, int attacker, double amount);
    void fire(int id);
    bool walkable(QPointF pos) const;
    void move(QPointF& pos, QPointF delta);
    double ray(QPointF pos, double angle, double limit=40) const;
    void write(QTcpSocket* socket, const QJsonObject& object);
    void parseClient(const QJsonObject& object);
    void updateClock();
    QTcpServer m_server;
    QTcpSocket m_client;
    std::array<QTcpSocket*,4> m_peers {{nullptr,nullptr,nullptr,nullptr}};
    std::array<QByteArray,4> m_peerBuffers;
    std::array<NetInput,4> m_inputs;
    std::array<qint64,4> m_inputTimes {{0,0,0,0}};
    QByteArray m_clientBuffer;
    ArenaGrid m_grid {};
    NetSnapshot m_authority, m_snapshot;
    QTimer m_timer;
    QElapsedTimer m_clock;
    qint64 m_last = 0;
    double m_sendAccumulator = 0;
    QString m_name, m_status = "Host a match or join a host on your Wi-Fi.";
    int m_localId = -1;
    bool m_leaving = false;
};
