﻿#include "mudmap.h"
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

MudMap::MudMap(QGraphicsScene *scene) : QGraphicsView(scene),
    m_isloading(false),
    m_hasPendingLoad(false)
{
    qRegisterMetaType<MudMap::TileSpec>("MudMap::TileSpec");
    m_mapThread = new MudMapThread;
    //
    connect(this, &MudMap::tileRequested, m_mapThread, &MudMapThread::requestTile, Qt::QueuedConnection);
    connect(m_mapThread, &MudMapThread::tileToAdd, this->scene(), &QGraphicsScene::addItem, Qt::QueuedConnection);
    connect(m_mapThread, &MudMapThread::tileToRemove, this->scene(), &QGraphicsScene::removeItem, Qt::QueuedConnection);
    connect(m_mapThread, &MudMapThread::tileToAdd, this, [&](QGraphicsItem* item){ m_tiles.insert(item); }, Qt::QueuedConnection);
    connect(m_mapThread, &MudMapThread::tileToRemove, this, [&](QGraphicsItem* item){ m_tiles.remove(item); }, Qt::QueuedConnection);
    connect(m_mapThread, &MudMapThread::requestFinished, this, [&](){
        m_isloading = false;
        if(m_hasPendingLoad) {
            updateTile();
            m_hasPendingLoad = false;
        }
    }, Qt::QueuedConnection);
    QString fileName = QString("E:/arcgis/%1/%2/%3.jpg")
            .arg(0)
            .arg(0)
            .arg(0);
    this->scene()->addPixmap(fileName);
    scale(2, 2);
}

MudMap::~MudMap()
{
    // 在此处从场景移出瓦片，防止和多线程析构冲突
    for(auto item : m_tiles) {
        this->scene()->removeItem(item);
    }
    delete m_mapThread;
}

void MudMap::wheelEvent(QWheelEvent *e)
{
    qreal curZoom = qLn(transform().m11()) / qLn(2);
    //
    qreal scaleFac = e->delta() * 0.01;
    qreal scaleXY = 1 + scaleFac;
    qreal newZoom = curZoom * scaleXY;
    if(newZoom <= 1 || newZoom >= 20)
        return;

    this->scale(scaleXY, scaleXY);
    if(m_isloading)
        m_hasPendingLoad = true;
    else
        updateTile();
}

void MudMap::mouseMoveEvent(QMouseEvent *event)
{
    QGraphicsView::mouseMoveEvent(event);
    if(m_isloading)
        m_hasPendingLoad = true;
    else
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
    int xBegin = topLeftPos.x() / 256 * tileLen - 1;
    int yBegin = topLeftPos.y() / 256 * tileLen - 1;
    int xEnd = bottomRightPos.x() / 256 * tileLen + 1;
    int yEnd = bottomRightPos.y() / 256 * tileLen + 1;
    if(xBegin < 0) xBegin = 0;
    if(yBegin < 0) yBegin = 0;
    if(xEnd >= tileLen) xEnd = tileLen - 1;
    if(yEnd >= tileLen) yEnd = tileLen - 1;
    m_isloading = true;
    emit tileRequested({intZoom, xBegin, yBegin}, {intZoom, xEnd, yEnd});

}

MudMapThread::TileCacheNode::~TileCacheNode()
{
    delete value;
}

MudMapThread::MudMapThread() :
    m_preTopLeft({0, 0, 0}),
    m_preBottomRight({0, 0, 0})
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
    if(m_preTopLeft == topLeft && m_preBottomRight == bottomRight) {
        emit requestFinished();
        return;
    }

    // y向下递增
    QSet<MudMap::TileSpec> curViewSet;
    QSet<MudMap::TileSpec> preViewSet;
    {
        const int &zoom = topLeft.zoom;
        const int xBegin = topLeft.x;
        const int xEnd = bottomRight.x;
        const int yBegin = topLeft.y;
        const int yEnd = bottomRight.y;
        for(int x = xBegin; x <= xEnd; ++x) {
            for(int y = yBegin; y <= yEnd; ++y) {
                curViewSet.insert({zoom, x, y});
            }
        }
    }
    {
        const int &zoom = m_preTopLeft.zoom;
        const int xBegin = m_preTopLeft.x;
        const int xEnd = m_preBottomRight.x;
        const int yBegin = m_preTopLeft.y;
        const int yEnd = m_preBottomRight.y;
        for(int x = xBegin; x <= xEnd; ++x) {
            for(int y = yBegin; y <= yEnd; ++y) {
                preViewSet.insert({zoom, x, y});
            }
        }
    }

    m_preTopLeft = topLeft;
    m_preBottomRight = bottomRight;

    // compute which to load and which to unload
    QSet<MudMap::TileSpec> needToShowTileSet;
    QSet<MudMap::TileSpec> needToHideTileSet;
    {
        QSetIterator<MudMap::TileSpec> iter(curViewSet);
        while (iter.hasNext()) {
            auto tileSpec = iter.next();
            createAscendingTileCache(tileSpec, needToShowTileSet);
        }
    }
    {
        QSetIterator<MudMap::TileSpec> iter(preViewSet);
        while (iter.hasNext()) {
            auto tileSpec = iter.next();
            createAscendingTileCache(tileSpec, needToHideTileSet);
        }
    }
    QSet<MudMap::TileSpec> realToHideTileSet = needToHideTileSet - needToShowTileSet;

    // update the scene tiles
    {
        QSetIterator<MudMap::TileSpec> iter(needToShowTileSet);
        while (iter.hasNext()) {
            auto &tileSpec = iter.next();
            showItem(tileSpec);
        }
    }
    {
        QSetIterator<MudMap::TileSpec> iter(realToHideTileSet);
        while (iter.hasNext()) {
            auto &tileSpec = iter.next();
            hideItem(tileSpec);
        }
    }

    emit requestFinished();
}

void MudMapThread::showItem(const MudMap::TileSpec &tileSpec)
{
    if(m_tileShowedSet.contains(tileSpec))
        return;

    auto tileItem = m_tileCache.object(tileSpec);
    if(tileItem->value) {
        emit tileToAdd(tileItem->value);
        m_tileShowedSet.insert(tileSpec);
    }
}

void MudMapThread::hideItem(const MudMap::TileSpec &tileSpec)
{
    // 看不见的直接不管
    if(!m_tileShowedSet.contains(tileSpec))
        return;

    auto tileItem = m_tileCache.object(tileSpec);
    if(tileItem->value) {
        emit tileToRemove(tileItem->value);
        m_tileShowedSet.remove(tileSpec);
    }
}

QGraphicsPixmapItem *MudMapThread::loadTileItem(const MudMap::TileSpec &tileSpec)
{
    const int tileOff = 256;
    int tileLen = qPow(2, tileSpec.zoom);
    //
    QString fileName = QString("E:/arcgis/%1/%2/%3.jpg")
            .arg(tileSpec.zoom)
            .arg(tileSpec.x)
            .arg(tileLen - tileSpec.y -1);
    if(!QFileInfo::exists(fileName))
        return nullptr;

    auto tileItem = new QGraphicsPixmapItem(fileName);
    tileItem->setZValue(tileSpec.zoom);

    //
    double xOff = tileOff * tileSpec.x;
    double yOff = tileOff * tileSpec.y;
    double scaleFac = 1.0 / tileLen;
    QTransform transform;
    transform.scale(scaleFac, scaleFac)
            .translate(xOff, yOff);
    tileItem->setTransform(transform);
    return tileItem;
}

void MudMapThread::createAscendingTileCache(const MudMap::TileSpec &tileSpec, QSet<MudMap::TileSpec> &sets)
{
    auto tileCacheItem = m_tileCache.object(tileSpec);

    //
    if(!tileCacheItem) {
        tileCacheItem = new MudMapThread::TileCacheNode;
        tileCacheItem->value = loadTileItem(tileSpec);
        tileCacheItem->tileSpec = tileSpec;
        m_tileCache.insert(tileSpec, tileCacheItem);
    }
    sets.insert(tileSpec);

    if(!tileCacheItem->value)
        createAscendingTileCache(tileSpec.rise(), sets);
}
