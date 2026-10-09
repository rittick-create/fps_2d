#include <QApplication>
#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QImage>
#include <QColor>
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSettings>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <array>
#include <vector>
#include <random>
#include <cmath>
#include <iostream>
#include <memory>
#include <queue>
#include "soundbank.h"

// Keep the production interface small while testing the simulation without a
// window, audio device, timer scheduling, or a separate Qt Test dependency.
#define private public
#define protected public
#include "gamewidget.h"
#undef protected
#undef private

namespace {
int checks = 0;
int failures = 0;
#define CHECK(condition) do { ++checks; if (!(condition)) { ++failures; \
    std::cerr << __func__ << ':' << __LINE__ << ": " #condition "\n"; } } while (false)

bool near(double a, double b, double epsilon = 0.0001) { return std::abs(a - b) < epsilon; }

std::unique_ptr<GameWidget> game() {
    auto g = std::make_unique<GameWidget>();
    g->m_timer.stop();
    g->m_rng.seed(2026);
    return g;
}

void emptyArena(GameWidget& g) {
    for (int y = 0; y < GameWidget::MapSize; ++y)
        for (int x = 0; x < GameWidget::MapSize; ++x)
            g.m_map[y][x] = x == 0 || y == 0 || x == GameWidget::MapSize - 1 || y == GameWidget::MapSize - 1 ? 1 : 0;
    g.m_enemies.clear();
    g.m_pickups.clear();
    g.m_barrels.clear();
    g.m_projectiles.clear();
    g.m_effects.clear();
    g.m_player = QPointF(3.5, 3.5);
    g.m_angle = 0;
    g.m_state = GameWidget::State::Playing;
    g.m_wave = 1;
    g.m_health = 100;
    g.m_shield = 0;
    g.m_firing = false;
    g.m_keys.clear();
    g.m_fireCooldown = 0;
    g.m_reloadTimer = 0;
}

void addEnemy(GameWidget& g, QPointF pos, double health = 60) {
    Enemy e;
    e.id = g.m_nextEnemyId++;
    e.pos = pos;
    e.health = e.maxHealth = health;
    e.attackTimer = 1000;
    g.m_enemies.push_back(e);
}

void runFor(GameWidget& g, double seconds) {
    while (seconds > 0) {
        double dt = std::min(seconds, 0.025);
        g.advance(dt);
        seconds -= dt;
    }
}

void mapAndPaths() {
    auto g = game();
    g->startGame();
    for (int i = 0; i < GameWidget::MapSize; ++i) {
        CHECK(g->tile(i, 0) != 0);
        CHECK(g->tile(i, GameWidget::MapSize - 1) != 0);
        CHECK(g->tile(0, i) != 0);
        CHECK(g->tile(GameWidget::MapSize - 1, i) != 0);
    }
    CHECK(g->tile(-1, 3) != 0);
    CHECK(g->tile(GameWidget::MapSize, 3) != 0);
    CHECK(g->walkable(g->m_player));
    std::array<std::array<bool, GameWidget::MapSize>, GameWidget::MapSize> seen {};
    std::queue<QPoint> todo;
    QPoint start(int(g->m_player.x()), int(g->m_player.y()));
    seen[start.y()][start.x()] = true;
    todo.push(start);
    const QPoint offsets[] = {QPoint(1, 0), QPoint(-1, 0), QPoint(0, 1), QPoint(0, -1)};
    while (!todo.empty()) {
        QPoint p = todo.front(); todo.pop();
        for (auto offset : offsets) {
            QPoint next = p + offset;
            if (g->tile(next.x(), next.y()) == 0 && !seen[next.y()][next.x()]) {
                seen[next.y()][next.x()] = true;
                todo.push(next);
            }
        }
    }
    int reachable = 0;
    for (int y = 0; y < GameWidget::MapSize; ++y)
        for (int x = 0; x < GameWidget::MapSize; ++x)
            if (g->tile(x, y) == 0) {
                CHECK(seen[y][x]);
                if (seen[y][x]) ++reachable;
            }
    CHECK(reachable > 100);
    for (const auto& e : g->m_enemies) {
        CHECK(g->walkable(e.pos));
        CHECK(seen[int(e.pos.y())][int(e.pos.x())]);
    }
    g->updatePaths();
    CHECK(g->m_path[start.y()][start.x()] == 0);
}

void raycastingAndCollision() {
    auto g = game();
    emptyArena(*g);
    constexpr double pi = 3.14159265358979323846;
    CHECK(near(g->castRay(QPointF(3.5, 3.5), 0), 19.5));
    CHECK(near(g->castRay(QPointF(3.5, 3.5), pi / 2), 19.5));
    CHECK(near(g->castRay(QPointF(3.5, 3.5), pi), 2.5));
    CHECK(near(g->castRay(QPointF(3.5, 3.5), -pi / 2), 2.5));
    CHECK(g->clearLine(QPointF(3.5, 3.5), QPointF(8.5, 3.5)));
    for (int y = 1; y < GameWidget::MapSize - 1; ++y) g->m_map[y][6] = 1;
    CHECK(near(g->castRay(QPointF(3.5, 3.5), 0), 2.5));
    CHECK(!g->clearLine(QPointF(3.5, 3.5), QPointF(8.5, 3.5)));
    CHECK(!g->walkable(QPointF(6.5, 3.5)));
    QPointF p(5.5, 3.5);
    for (int i = 0; i < 50; ++i) g->moveActor(p, QPointF(0.1, 0), 0.2);
    CHECK(p.x() <= 5.80001);
    CHECK(g->walkable(p));
    double beforeY = p.y();
    for (int i = 0; i < 10; ++i) g->moveActor(p, QPointF(0.1, 0.1), 0.2);
    CHECK(p.x() <= 5.80001);
    CHECK(p.y() > beforeY + 0.5);
}

void shootingAndScoring() {
    auto g = game();
    emptyArena(*g);
    addEnemy(*g, QPointF(6.5, 3.5), 1);
    int ammo = g->m_ammo[0];
    g->shoot();
    CHECK(!g->m_enemies[0].alive);
    CHECK(g->m_kills == 1);
    CHECK(g->m_score > 0);
    CHECK(g->m_ammo[0] == ammo - 1);
    CHECK(g->m_shots == 1);
    CHECK(g->m_hits == 1);
    int score = g->m_score;
    g->hurtEnemy(0, 1000);
    CHECK(g->m_kills == 1);
    CHECK(g->m_score == score);
    g->shoot();
    CHECK(g->m_ammo[0] == ammo - 1); // Fire-rate limit.

    emptyArena(*g);
    addEnemy(*g, QPointF(8.5, 3.5));
    g->m_map[3][6] = 1;
    g->shoot();
    CHECK(g->m_enemies[0].health == 60);
    CHECK(g->m_enemies[0].alive);
    CHECK(g->m_kills == 1);
    g->m_fireCooldown = 0;
    g->m_ammo[0] = 0;
    int shots = g->m_shots;
    g->shoot();
    CHECK(g->m_shots == shots);
    CHECK(g->m_ammo[0] == 0);
}

void reloadAndWeaponSwitch() {
    auto g = game();
    emptyArena(*g);
    addEnemy(*g, QPointF(20.5, 20.5));
    g->m_ammo[0] = 2;
    g->m_reserve[0] = 5;
    g->reload();
    CHECK(g->m_reloadTimer > 0);
    int shots = g->m_shots;
    g->shoot();
    CHECK(g->m_shots == shots);
    runFor(*g, 4);
    CHECK(g->m_reloadTimer == 0);
    CHECK(g->m_ammo[0] == 7);
    CHECK(g->m_reserve[0] == 0);
    g->reload();
    CHECK(g->m_reloadTimer == 0);

    g->m_ammo[0] = 1;
    g->m_reserve[0] = 20;
    g->reload();
    CHECK(g->m_reloadTimer > 0);
    g->switchWeapon(1);
    CHECK(g->m_weapon == 1);
    CHECK(g->m_reloadTimer == 0);
    CHECK(g->m_ammo[0] == 1);
    CHECK(g->m_reserve[0] == 20);
    g->m_ammo[1] = 0;
    g->m_reserve[1] = 2;
    g->reload();
    runFor(*g, 4);
    CHECK(g->m_ammo[1] == 2);
    CHECK(g->m_reserve[1] == 0);
}

void pauseAndReset() {
    auto g = game();
    g->m_difficulty = 2;
    g->startGame();
    CHECK(g->m_state == GameWidget::State::Playing);
    CHECK(g->m_wave == 1);
    CHECK(g->m_difficulty == 2);
    CHECK(g->aliveEnemies() > 0);
    g->m_keys.insert(Qt::Key_W);
    g->m_firing = true;
    g->pauseGame();
    CHECK(g->m_state == GameWidget::State::Paused);
    CHECK(g->m_keys.isEmpty());
    CHECK(!g->m_firing);
    auto player = g->m_player;
    auto health = g->m_health;
    auto time = g->m_time;
    auto ammo = g->m_ammo;
    for (int i = 0; i < 10; ++i) g->tick();
    CHECK(g->m_player == player);
    CHECK(g->m_health == health);
    CHECK(g->m_time == time);
    CHECK(g->m_ammo == ammo);
    g->resumeGame();
    CHECK(g->m_state == GameWidget::State::Playing);
    g->m_score = 123;
    g->m_kills = 6;
    g->m_health = 1;
    g->m_damageScale = 2;
    g->m_reloadScale = 0.4;
    g->m_difficulty = 0;
    g->startGame();
    CHECK(g->m_difficulty == 0);
    CHECK(g->m_wave == 1);
    CHECK(g->m_score == 0);
    CHECK(g->m_kills == 0);
    CHECK(g->m_health == g->m_maxHealth);
    CHECK(g->m_time == 0);
    CHECK(g->m_damageScale == 1);
    CHECK(g->m_reloadScale == 1);
    CHECK(g->m_projectiles.empty());
}

void pickupsAndDefeat() {
    auto g = game();
    emptyArena(*g);
    addEnemy(*g, QPointF(20.5, 20.5));
    g->m_health = 40;
    g->m_shield = 0;
    g->m_reserve = {{0,0}};
    g->m_pickups.push_back({Pickup::Health, g->m_player, true});
    g->m_pickups.push_back({Pickup::Shield, g->m_player, true});
    g->m_pickups.push_back({Pickup::Ammo, g->m_player, true});
    g->advance(0.025);
    CHECK(g->m_health > 40);
    CHECK(g->m_health <= g->m_maxHealth);
    CHECK(g->m_shield > 0);
    CHECK(g->m_reserve[0] > 0);
    for (const auto& p : g->m_pickups) CHECK(!p.active);
    double health = g->m_health;
    auto reserve = g->m_reserve;
    g->advance(0.025);
    CHECK(g->m_health == health);
    CHECK(g->m_reserve == reserve);
    g->m_shield = 20;
    g->hurtPlayer(10);
    CHECK(g->m_shield < 20);
    CHECK(near(g->m_health, health - 3.5)); // Shields absorb 65%; health takes the rest.
    g->m_shield = 0;
    g->hurtPlayer(10000);
    CHECK(g->m_state == GameWidget::State::Lost);
    CHECK(g->m_health == 0);
    CHECK(!g->m_firing);
}

void wavesAndVictory() {
    auto g = game();
    g->startGame();
    for (int wave = 1; wave <= 5; ++wave) {
        CHECK(g->m_wave == wave);
        CHECK(g->m_state == GameWidget::State::Playing);
        CHECK(g->aliveEnemies() > 0);
        bool hasBoss = false;
        for (const auto& e : g->m_enemies) if (e.kind == Enemy::Sentinel) hasBoss = true;
        if (wave == 5) CHECK(hasBoss);
        for (int i = 0; i < int(g->m_enemies.size()); ++i) g->hurtEnemy(i, 100000);
        g->advance(0.025);
        CHECK(g->aliveEnemies() == 0);
        if (wave < 5) {
            CHECK(g->m_state == GameWidget::State::Upgrade);
            double damage = g->m_damageScale;
            g->activateAction(0);
            CHECK(g->m_damageScale > damage);
        }
    }
    CHECK(g->m_state == GameWidget::State::Won);
    CHECK(g->m_score > 0);
    CHECK(g->difficultyBest() >= g->m_score);
    CHECK(!g->m_firing);
}

void inactiveControls() {
    auto g = game();
    for (auto state : {GameWidget::State::Menu, GameWidget::State::Paused,
                       GameWidget::State::Upgrade, GameWidget::State::Won, GameWidget::State::Lost}) {
        g->m_state = state;
        g->m_ammo = {{3,2}};
        g->m_reserve = {{10,10}};
        g->m_reloadTimer = 0;
        g->m_fireCooldown = 0;
        int shots = g->m_shots;
        g->shoot();
        g->reload();
        CHECK(g->m_ammo[0] == 3);
        CHECK(g->m_shots == shots);
        CHECK(g->m_reloadTimer == 0);
        QKeyEvent reloadKey(QEvent::KeyPress, Qt::Key_R, Qt::NoModifier);
        g->keyPressEvent(&reloadKey);
        CHECK(g->m_reloadTimer == 0);
        QKeyEvent moveKey(QEvent::KeyPress, Qt::Key_W, Qt::NoModifier);
        g->keyPressEvent(&moveKey);
        CHECK(!g->m_keys.contains(Qt::Key_W));
    }
}
}

int main(int argc, char** argv) {
    qputenv("GRIDBREACH_TEST", "1");
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("GridBreachTests");
    QCoreApplication::setApplicationName("isolated-checks");
    QTemporaryDir settingsDirectory;
    if (!settingsDirectory.isValid()) return 2;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settingsDirectory.path());
    mapAndPaths();
    raycastingAndCollision();
    shootingAndScoring();
    reloadAndWeaponSwitch();
    pauseAndReset();
    pickupsAndDefeat();
    wavesAndVictory();
    inactiveControls();
    if (app.arguments().contains("--screenshots")) {
        auto g=game(); g->show();app.processEvents();
        g->grab().save("menu.png");g->startGame();g->m_timer.stop();
        g->m_bannerTimer=0;g->grab().save("gameplay.png");
        g->pauseGame();g->grab().save("pause.png");
        g->m_state=GameWidget::State::Upgrade;g->grab().save("upgrade.png");
        g->m_state=GameWidget::State::Won;g->m_score=8270;g->m_kills=42;g->m_wave=5;g->grab().save("results.png");
        g->m_state=GameWidget::State::Menu;g->resize(1024,720);app.processEvents();g->grab().save("menu-small.png");
    }
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
