#include <mudmap.h>
#include <QApplication>
#include <QOpenGLWidget>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MudMap map(new QGraphicsScene);
    map.setTilePath("C:/Users/Yshin/Downloads/Arcgis");
    map.setDragMode(QGraphicsView::ScrollHandDrag);
    map.setRenderHint(QPainter::Antialiasing, true);
    map.setOptimizationFlags(QGraphicsView::DontSavePainterState);
    map.setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    map.setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    map.setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
    map.setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
    map.setViewport(new QOpenGLWidget);
    map.show();
    return a.exec();
}
