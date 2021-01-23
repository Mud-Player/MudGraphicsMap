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
    virtual void wheelEvent(QWheelEvent *e) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;

private:
    void updateTile();
private:
    MudMapThread        *m_mapThread;
    QSet<QGraphicsItem*> m_tiles;
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
    struct TileCacheNode {
        MudMap::TileSpec tileSpec;
        QGraphicsItem *value = nullptr;
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
    void showItem(const MudMap::TileSpec &tileSpec);
    void hideItem(const MudMap::TileSpec &tileSpec);
    /// 从磁盘加载瓦片文件
    QGraphicsPixmapItem* loadTileItem(const MudMap::TileSpec &tileSpec);
    void createAscendingTileCache(const MudMap::TileSpec &tileSpec, QSet<MudMap::TileSpec> &sets);

private:
    QCache<MudMap::TileSpec, TileCacheNode> m_tileCache; ///<已加载瓦片缓存
    QSet<MudMap::TileSpec>    m_tileShowedSet;           ///<已显示瓦片编号集合(存在依赖关系的瓦片，实际上只有顶层才显示，但是子瓦片仍然被当作Showed)
    //
    MudMap::TileSpec m_preTopLeft;
    MudMap::TileSpec m_preBottomRight;
};

#endif // MUDMAP_H
