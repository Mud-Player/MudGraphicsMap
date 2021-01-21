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
    qlonglong lhsVal = (qlonglong(this->zoom)<<48) + (qlonglong(this->x)<< 24) + this->y;
    qlonglong rhsVal = (qlonglong(rhs.zoom)<<48) + (qlonglong(rhs.x)<< 24) + rhs.y;
    return  lhsVal < rhsVal;
}

bool MudMap::TileSpec::operator==(const MudMap::TileSpec &rhs) const
{
    return (this->zoom == rhs.zoom) && (this->x == rhs.x) && (this->y == rhs.y);
}

MudMap::MudMap(QGraphicsScene *scene) : QGraphicsView(scene)
{
    qRegisterMetaType<MudMap::TileSpec>("MudMap::TileSpec");
    m_mapThread = new MudMapThread;
    connect(this, &MudMap::tileRequested, m_mapThread, &MudMapThread::requestTile, Qt::QueuedConnection);
    connect(m_mapThread, &MudMapThread::tileToAdd, this->scene(), &QGraphicsScene::addItem, Qt::QueuedConnection);
    connect(m_mapThread, &MudMapThread::tileToRemove, this->scene(), &QGraphicsScene::removeItem, Qt::QueuedConnection);
    QString fileName = QString("E:/arcgis/%1/%2/%3.jpg")
            .arg(0)
            .arg(0)
            .arg(0);
    this->scene()->addPixmap(fileName);
}

MudMap::~MudMap()
{
    delete m_mapThread;
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
    int xBegin = topLeftPos.x() / 256 * tileLen - 1;
    int xEnd = bottomRightPos.x() / 256 * tileLen + 1;
    int yBegin = topLeftPos.y() / 256 * tileLen - 1;
    int yEnd = bottomRightPos.y() / 256 * tileLen + 1;
    emit tileRequested({intZoom, xBegin, yBegin}, {intZoom, xEnd, yEnd});

}

MudMapThread::MudMapThread()
{
    QThread *thread = new QThread;
    thread->setObjectName("MapThread");
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
    for(int x = xBegin; x <= xEnd; ++x) {
        for(int y = yBegin; y <= yEnd; ++y) {
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
            m_tileSpecSet.insert(tileSpec); // add 1
            m_tiles.insert(tileSpec, tileItem); // add 2
            emit tileToAdd(tileItem);
        }
    }

    // need to remove from scene
    {
        auto invisibleTiles = m_tileSpecSet - visibleTilesSet;
        QSetIterator<MudMap::TileSpec> i(invisibleTiles);
        while (i.hasNext()) {
            auto tileSpec = i.next();
            m_tileSpecSet.remove(tileSpec);     // remove 1
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
