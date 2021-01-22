#include "mudmap.h"
#include <QOpenGLWidget>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QDebug>
#include <QGraphicsLineItem>
#include <QtMath>
#include <QThread>
#include <QFileInfo>

MudMap::TileSpec MudMap::TileSpec::rise() const
{
    return MudMap::TileSpec({this->zoom-1, this->x/2, this->y/2});
}

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
    updateTile();
}

void MudMap::updateTile()
{
    qreal curZoom = qLn(transform().m11()) / qLn(2);
    int intZoom = qCeil(curZoom);
    //
    int tileLen = qPow(2, intZoom);
    auto topLeftPos = mapToScene(viewport()->geometry().topLeft());
    auto bottomRightPos = mapToScene(viewport()->geometry().bottomRight());
    int xBegin = topLeftPos.x() / 256 * tileLen;
    int yBegin = topLeftPos.y() / 256 * tileLen;
    int xEnd = bottomRightPos.x() / 256 * tileLen;
    int yEnd = bottomRightPos.y() / 256 * tileLen;
    if(xBegin < 0) xBegin = 0;
    if(yBegin < 0) yBegin = 0;
    if(xEnd >= tileLen) xEnd = tileLen - 1;
    if(yEnd >= tileLen) yEnd = tileLen - 1;
    emit tileRequested({intZoom, xBegin, yBegin}, {intZoom, xEnd, yEnd});

}

MudMapThread::TileCacheNode::~TileCacheNode()
{
    qDebug()<<"Destroy"<<this->tileSpec.zoom << this->tileSpec.x << this->tileSpec.y;
    if(parent)
        parent->refCount -= 1;
    if(refCount >= 1)
        qDebug()<<"refCount :" <<refCount;
    delete value;
}

MudMapThread::MudMapThread()
{
    m_tileCache.setMaxCost(1000);
    //
    QThread *thread = new QThread;
    thread->setObjectName("MapThread");
    this->moveToThread(thread);
    thread->start();
}

MudMapThread::~MudMapThread()
{
    this->thread()->quit();
    this->thread()->wait();
    delete this->thread();
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
    QSet<MudMap::TileSpec> insideTilesSet;
    for(int x = xBegin; x <= xEnd; ++x) {
        for(int y = yBegin; y <= yEnd; ++y) {
            insideTilesSet.insert({zoom, x, y});
        }
    }

    // need to load new tile to scene
    {
        auto newTiles = insideTilesSet - m_tileRequestedSet;
        QSetIterator<MudMap::TileSpec> i(newTiles);
        qDebug()<<"NewTiles Count: +" << +newTiles.count();
        while (i.hasNext()) {
            auto tileSpec = i.next();
            updateRequestedTile(tileSpec);
        }
    }

    // need to remove from scene
    {
        auto outsideTiles = m_tileRequestedSet - insideTilesSet;
        QSetIterator<MudMap::TileSpec> i(outsideTiles);
        qDebug()<<"OldTiles Count: -" << outsideTiles.count();
        while (i.hasNext()) {
            auto tileSpec = i.next();
            updateElapsedTile(tileSpec);
        }
    }

    //
    m_tileRequestedSet = insideTilesSet;
}

void MudMapThread::updateRequestedTile(const MudMap::TileSpec &tileSpec)
{
    //
    auto tileCacheItem = m_tileCache.object(tileSpec);
    MudMapThread::TileCacheNode* tileItem = nullptr;

    //
    if(tileCacheItem) {
        tileItem = createTileCacheRecursively(tileCacheItem);
    }
    else {
        tileItem = createTileCache(tileSpec);
        tileItem = createTileCacheRecursively(tileItem);
    }

    //
    if(!tileItem->show) {
        tileItem->show = true;
        emit tileToAdd(tileItem->value);
        qDebug()<<"CCC" << tileSpec.zoom << tileSpec.x << tileSpec.y
               << "||" << tileItem->tileSpec.zoom << tileItem->tileSpec.x << tileItem->tileSpec.y;
    }
}

void MudMapThread::updateElapsedTile(const MudMap::TileSpec &tileSpec)
{
    auto tileCacheItem = m_tileCache.object(tileSpec);
    if(!tileCacheItem)
        return;
    //
    qDebug()<<"Unload: " << tileSpec.zoom << tileSpec.x << tileSpec.y;
    unloadTile(tileCacheItem);
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
    if(!QFileInfo::exists(fileName))
        return nullptr;

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

void MudMapThread::unloadTile(MudMapThread::TileCacheNode *node)
{
    auto parent = node->parent;

    // tells scene to remove the tile as nobody need it
    if(node->show) { //ref change from 1 to 0, means it's shown now
        node->show = false;
        emit tileToRemove(node->value);
        qDebug()<<"RRRR" << node->tileSpec.zoom << node->tileSpec.x << node->tileSpec.y;
    }

    //  tell parent that such node don't need it anymore
    if(parent) {
        parent->refCount -= 1;
        // if nodody need parent, remove the parent too
        unloadTile(node->parent);
    }
}

MudMapThread::TileCacheNode *MudMapThread::createTileCacheRecursively(MudMapThread::TileCacheNode *child)
{
    if(!child) {
        qWarning() << __FUNCTION__ <<"@Child has be a vaild pointer";
        return nullptr;
    }

    if(child->value)
        return child;

    //
    auto tileSpec = child->tileSpec.rise();

    MudMapThread::TileCacheNode *parent = nullptr;
    if(m_tileCache.contains(tileSpec)) {
        parent = m_tileCache.object(tileSpec);
        parent->refCount += 1;
    }
    else {
        parent = new MudMapThread::TileCacheNode;
        parent->tileSpec = tileSpec;
        parent->refCount = 1;
        parent->value = loadTile(tileSpec);
        m_tileCache.insert(tileSpec, parent);
    }

    child->parent = parent;

    //
    if(!parent->value) {
        if(tileSpec.zoom == 0) {
            return nullptr;
        }
        else {
            return createTileCacheRecursively(parent);
        }
    }

    //
    return parent;
}

MudMapThread::TileCacheNode *MudMapThread::createTileCache(const MudMap::TileSpec &tileSpec)
{
    MudMapThread::TileCacheNode *tile = new MudMapThread::TileCacheNode;
    tile->value = loadTile(tileSpec);
    tile->parent = nullptr;
    tile->refCount = 0;
    tile->tileSpec = tileSpec;
    m_tileCache.insert(tileSpec, tile);
    return tile;
}

