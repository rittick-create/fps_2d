#include <QApplication>
#include "gamewidget.h"
int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setOrganizationName("CG Lab");
    app.setApplicationName("Grid Breach 2D");
    app.setApplicationVersion("1.0.0");
    GameWidget game;
    game.show();
    return app.exec();
}
