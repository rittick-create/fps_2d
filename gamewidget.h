#pragma once
#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QImage>
#include <QColor>
#include <array>
#include <vector>
#include <random>
#include "soundbank.h"

struct Enemy {
    enum Kind { Guard, Runner, Bomber, Sentinel };
    int id = 0;
    Kind kind = Guard;
    QPointF pos;
    double health = 60, maxHealth = 60, attackTimer = 1, hitFlash = 0;
    bool alive = true;
};
struct Pickup { enum Kind { Ammo, Health, Shield }; Kind kind; QPointF pos; bool active = true; };
struct Barrel { QPointF pos; bool active = true; };
struct Projectile { QPointF pos, velocity; double life = 4; int damage = 10; };
struct WorldEffect { QPointF pos; double life, maxLife; QColor color; double size; };

class GameWidget : public QWidget {
    Q_OBJECT
public:
    explicit GameWidget(QWidget* parent = nullptr);
    ~GameWidget() override;
protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void keyReleaseEvent(QKeyEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void focusOutEvent(QFocusEvent*) override;
private:
    enum class State { Menu, Playing, Paused, Upgrade, Won, Lost };
    static constexpr int MapSize = 24;
    void tick();
    void advance(double dt);
    void startGame();
    void startWave();
    void finishGame(bool won);
    void pauseGame();
    void resumeGame();
    void setMouseLook(bool enabled);
    void activateAction(int index);
    void shoot();
    void reload();
    void switchWeapon(int weapon);
    void hurtPlayer(int damage);
    void hurtEnemy(int index, double damage);
    void explode(QPointF pos, double radius, double damage, bool hurtSelf);
    void addEffect(QPointF pos, QColor color, double size = 0.5);
    void moveActor(QPointF& pos, QPointF delta, double radius);
    bool walkable(QPointF pos, double radius = 0.2) const;
    bool clearLine(QPointF a, QPointF b) const;
    int tile(int x, int y) const;
    double castRay(QPointF pos, double angle, double limit = 40,
                   int* side = nullptr, int* wall = nullptr, double* u = nullptr) const;
    void updatePaths();
    int aliveEnemies() const;
    int difficultyBest() const;
    double random(double low, double high);
    QRectF viewportRect() const;
    QRectF actionRect(int index) const;
    void drawWorld(QPainter& painter, const QRectF& viewport);
    void drawHud(QPainter& painter, const QRectF& viewport);
    void drawOverlay(QPainter& painter);

    State m_state = State::Menu;
    int m_difficulty = 1; // 0 Easy, 1 Standard, 2 Hard
    std::array<int, 3> m_best {{0,0,0}};
    bool m_newBest = false, m_mouseLook = false, m_firing = false, m_showMap = true;
    bool m_testMode = false;
    QSet<int> m_keys;
    QTimer m_timer;
    QElapsedTimer m_clock;
    qint64 m_lastTick = 0;
    SoundBank m_sound;
    std::mt19937 m_rng {std::random_device{}()};
    std::array<std::array<int,MapSize>,MapSize> m_map {};
    std::array<std::array<int,MapSize>,MapSize> m_path {};
    QPointF m_player {3.5,3.5};
    double m_angle = 0, m_time = 0, m_pathTimer = 0;
    double m_health = 100, m_maxHealth = 100, m_shield = 25, m_stamina = 100;
    int m_wave = 0, m_score = 0, m_kills = 0, m_shots = 0, m_hits = 0;
    int m_nextEnemyId = 1, m_weapon = 0;
    std::array<int,2> m_ammo {{18,6}}, m_reserve {{90,24}};
    double m_fireCooldown = 0, m_reloadTimer = 0, m_reloadDuration = 1.4;
    double m_damageScale = 1, m_reloadScale = 1, m_dashCooldown = 0;
    double m_damageFlash = 0, m_muzzleFlash = 0, m_hitMarker = 0, m_recoil = 0, m_bob = 0;
    double m_bannerTimer = 0;
    QString m_banner;
    std::vector<Enemy> m_enemies;
    std::vector<Pickup> m_pickups;
    std::vector<Barrel> m_barrels;
    std::vector<Projectile> m_projectiles;
    std::vector<WorldEffect> m_effects;
};
