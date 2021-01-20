#include "mudmap.h"
#include <QOpenGLWidget>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QDebug>
#include <QGraphicsLineItem>
#include <QtMath>
#include <QThread>

bool MudMap::TileSpec::operator <(const MudMap::TileSpec &rhs) const
{
    return ((this->zoom<<24) + (this->x << 8) + this->y) < ((rhs.zoom<<24) + (rhs.x << 8) + rhs.y);
}

bool MudMap::TileSpec::operator==(const MudMap::TileSpec &rhs) const
{
    return (this->zoom == rhs.zoom) && (this->x == rhs.x) && (this->y == rhs.y);
}

MudMap::MudMap(QGraphicsScene *scene) : QGraphicsView(scene)
{
    qRegisterMetaType<MudMap::TileSpec>("MudMap::TileSpec");
    MudMapThread *mapThread = new MudMapThread;
    connect(this, &QObject::destroyed, mapThread, &MudMapThread::deleteLater);
    connect(this, &MudMap::tileRequested, mapThread, &MudMapThread::requestTile, Qt::QueuedConnection);
    connect(mapThread, &MudMapThread::tileToAdd, this->scene(), &QGraphicsScene::addItem, Qt::QueuedConnection);
    connect(mapThread, &MudMapThread::tileToRemove, this->scene(), &QGraphicsScene::removeItem, Qt::QueuedConnection);
    QString fileName = QString("E:/arcgis/%1/%2/%3.jpg")
            .arg(0)
            .arg(0)
            .arg(0);
    this->scene()->addPixmap(fileName);
}

void MudMap::wheelEvent(QWheelEvent *e)
{
    if (e->delta() > 0)
        scale(10.0/9, 10.0/9);
    else
        scale(9.0/10, 9.0/10);
    fitTile();
}

void MudMap::fitTile()
{
    qreal curZoom = qLn(transform().m11()) / qLn(2);
    int intZoom = qCeil(curZoom);
    //
    int tileLen = qPow(2, intZoom);
    auto topLeftPos = mapToScene(viewport()->geometry().topLeft());
    auto bottomRightPos = mapToScene(viewport()->geometry().bottomRight());
    int xBegin = topLeftPos.x() / 256 * tileLen;
    int xEnd = bottomRightPos.x() / 256 * tileLen;
    int yBegin = topLeftPos.y() / 256 * tileLen;
    int yEnd = bottomRightPos.y() / 256 * tileLen;
    emit tileRequested({intZoom, xBegin, yBegin}, {intZoom, xEnd, yEnd});

}

MudMapThread::MudMapThread()
{
    QThread *thread = new QThread;
    this->moveToThread(thread);
    thread->start();
}

void MudMapThread::requestTile(const MudMap::TileSpec &topLeft, const MudMap::TileSpec &bottomRight)
{
    if(m_preTopLeft == topLeft && m_preBottomRight == bottomRight)
        return;
    m_preTopLeft = topLeft;
    m_preBottomRight = bottomRight;

    // y向下递增
    const int &zoom = topLeft.zoom;
    int xBegin = topLeft.x;
    int xEnd = bottomRight.x;
    int yBegin = topLeft.y;
    int yEnd = bottomRight.y;
    QSet<MudMap::TileSpec> visibleTilesSet;
    for(int x = xBegin; x < xEnd; ++x) {
        for(int y = yBegin; y < yEnd; ++y) {
            visibleTilesSet.insert({zoom, x, y});
        }
    }

    // need to load new tile to scene
    {
        auto newTiles = visibleTilesSet - m_tileSpecSet;
        QSetIterator<MudMap::TileSpec> i(newTiles);
        while (i.hasNext()) {
            auto tileSpec = i.next();
            auto tileItem = loadTile(tileSpec);
            m_tileSpecSet.insert(tileSpec);
            m_tiles.insert(tileSpec, tileItem);
            emit tileToAdd(tileItem);
        }
    }

    // need to remove from scene
    {
        auto invisibleTiles = m_tileSpecSet - visibleTilesSet;
        QSetIterator<MudMap::TileSpec> i(invisibleTiles);
        while (i.hasNext()) {
            auto tileSpec = i.next();
            m_tileSpecSet.remove(tileSpec);     // remove1
            auto tileItem = m_tiles.take(tileSpec); // remove 2
            emit tileToRemove(tileItem);
        }
    }
}

MudMapThread::~MudMapThread()
{
    this->thread()->quit();
    this->thread()->wait();
    delete this->thread();
}

QGraphicsPixmapItem *MudMapThread::loadTile(const MudMap::TileSpec &tile)
{
    const int tileOff = 256;
    int tileLen = qPow(2, tile.zoom);
    //
    QString fileName = QString("E:/arcgis/%1/%2/%3.jpg")
            .arg(tile.zoom)
            .arg(tile.x)
            .arg(tileLen - tile.y -1);
    auto tileItem = new QGraphicsPixmapItem(fileName);
    tileItem->setZValue(tile.zoom);
    //
    double xOff = tileOff * tile.x;
    double yOff = tileOff * tile.y;
    double scaleFac = 1.0 / tileLen;
    QTransform transform;
    transform.scale(scaleFac, scaleFac)
            .translate(xOff, yOff);
    tileItem->setTransform(transform);
    return tileItem;
}
