#include "mudmap.h"
#include <QOpenGLWidget>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QDebug>
#include <QGraphicsLineItem>
#include <QtMath>

bool GraphicsView::TileSpec::operator <(const GraphicsView::TileSpec &rhs) const
{
    return ((this->zoom<<24) + (this->x << 8) + this->y) < ((rhs.zoom<<24) + (rhs.x << 8) + rhs.y);
}

GraphicsView::GraphicsView(QGraphicsScene *scene) : QGraphicsView(scene)
{
    addTile({0, 0, 0});
}

QGraphicsPixmapItem *GraphicsView::addTile(const GraphicsView::TileSpec &tile)
{
    const int tileOff = 256;
    if(m_tiles.contains(tile))
        return m_tiles.value(tile);
    //
    QString fileName = QString("E:/arcgis/%1/%2/%3.jpg")
            .arg(tile.zoom)
            .arg(tile.x)
            .arg(tile.y);
    auto tileItem = this->scene()->addPixmap(fileName);
    tileItem->setZValue(tile.zoom);
    //
    int rowColNum = qPow(2, tile.zoom);
    double xOff = tileOff * tile.x;
    double yOff = tileOff * (rowColNum - tile.y -1);
    double scaleFac = 1.0 / rowColNum;
    QTransform transform;
    transform.scale(scaleFac, scaleFac)
            .translate(xOff, yOff);
    tileItem->setTransform(transform);
    m_tiles.insert(tile, tileItem);
    return tileItem;
}

void GraphicsView::wheelEvent(QWheelEvent *e)
{
    if (e->delta() > 0)
        scale(5.0/4, 5.0/4);
    else
        scale(4.0/5, 4.0/5);
    fitTile();
}

void GraphicsView::fitTile()
{
    qreal curZoom = qLn(transform().m11()) / qLn(2);
    qDebug()<<curZoom;
    //
    int intZoom = int(curZoom);
    int tileLen = qPow(2, intZoom);
    auto topLeftPos = mapToScene(viewport()->geometry().topLeft());
    auto bottomRightPos = mapToScene(viewport()->geometry().bottomRight());
    int xBegin = topLeftPos.x() / 256 * tileLen;
    int xEnd = bottomRightPos.x() / 256 * tileLen;
    int yBegin = topLeftPos.y() / 256 * tileLen;
    int yEnd = bottomRightPos.y() / 256 * tileLen;
    qDebug()<<xBegin<<xEnd<<yBegin<<yEnd;
    for(int x = xBegin; x < xEnd; ++x) {
        for(int y = yBegin; y < yEnd; ++y) {
            addTile({intZoom, x, tileLen - y - 1});
        }
    }
}

MudMap::MudMap(QWidget *parent)
    : QWidget(parent)
{
    // scene and view
    m_scene = new QGraphicsScene(this);
    m_view = new GraphicsView(m_scene);
    m_view->setDragMode(QGraphicsView::ScrollHandDrag);
    m_view->setRenderHint(QPainter::Antialiasing, false);
    m_view->setOptimizationFlags(QGraphicsView::DontSavePainterState);
    m_view->setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
    m_view->setViewport(new QOpenGLWidget);

    // let view show full with parent(that's such class)
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->addWidget(m_view);
    layout->setContentsMargins(0, 0, 0, 0);
}

MudMap::~MudMap()
{
}
