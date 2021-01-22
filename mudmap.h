#ifndef MUDMAP_H
#define MUDMAP_H

#include <QWidget>
#include <QGraphicsView>
#include <QWheelEvent>
#include <QCache>

class MudMapThread;
/*!
 * \brief 基于Graphics View的地图
 * \details 其仅用于显示瓦片地图
 * \bug item没有释放
 */
class MudMap : public QGraphicsView
{
    Q_OBJECT

public:
    struct TileSpec {
        int zoom;
        int x;
        int y;
        TileSpec rise() const;
        bool operator< (const TileSpec &rhs) const;
        bool operator== (const TileSpec &rhs) const;
    };

    MudMap(QGraphicsScene *scene);
    ~MudMap();

signals:
    void tileRequested(const MudMap::TileSpec &topLeft, const MudMap::TileSpec &bottomRight);

protected:
    void wheelEvent(QWheelEvent *e) override;

private:
    void updateTile();
private:
    MudMapThread *m_mapThread;
};

inline uint qHash(const MudMap::TileSpec &key, uint seed)
{
    qlonglong keyVal = (qlonglong(key.zoom)<<48) + (qlonglong(key.x)<< 24) + key.y;
    return qHash(keyVal, seed);
}

/*!
 * \brief 瓦片地图管理线程
 * \details 负责加载瓦片、卸载瓦片
 */
class MudMapThread : public QObject
{
    Q_OBJECT
    /// 瓦片缓存节点，配合QCache实现缓存机制
public:
    struct TileCacheNode {
        MudMap::TileSpec tileSpec;
        QGraphicsItem *value = nullptr;    ///< 如果没有则为空，并且依赖其父节点提供图片
        TileCacheNode *parent = nullptr;   ///< 如果当前瓦片没有文件，则依赖上一层级的瓦片，依次递归
        int refCount = 0;   ///< 子节点引用计数(本节点要显示、本节点要引用其他节点、子节点引用该节点 >> 都要+1, 反之-1)
        bool show = false;          ///< 是否显示
        ~TileCacheNode();
    };

public:
    MudMapThread();
    ~MudMapThread();
public slots:
    void requestTile(const MudMap::TileSpec &topLeft, const MudMap::TileSpec &bottomRight);

signals:
    void tileToAdd(QGraphicsItem *tile);
    void tileToRemove(QGraphicsItem *tile);

private:
    void updateRequestedTile(const MudMap::TileSpec &tile);   ///< 可能存在递归
    void updateElapsedTile(const MudMap::TileSpec &tileSpec);
    /// 从磁盘加载瓦片文件
    QGraphicsPixmapItem* loadTile(const MudMap::TileSpec &tile);
    /// 递归卸载该瓦片节点及其依赖
    void unloadTile(MudMapThread::TileCacheNode *node);
    /// 递归创建瓦片依赖数,其引用计数已经更新
    TileCacheNode *createTileCacheRecursively(TileCacheNode *child);
    /// 创建一个瓦片
    TileCacheNode *createTileCache(const MudMap::TileSpec &tileSpec);


private:
    QCache<MudMap::TileSpec, TileCacheNode> m_tileCache; ///<已加载瓦片图元
    QSet<MudMap::TileSpec>    m_tileRequestedSet;             ///<请求加载瓦片编号集合(不包括依赖父节点)
    //
    MudMap::TileSpec m_preTopLeft;
    MudMap::TileSpec m_preBottomRight;
};

#endif // MUDMAP_H
