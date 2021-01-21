#ifndef MUDMAP_H
#define MUDMAP_H

#include <QWidget>
#include <QGraphicsView>
#include <QWheelEvent>

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
    void fitTile();
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
public:
    MudMapThread();
    void requestTile(const MudMap::TileSpec &topLeft, const MudMap::TileSpec &bottomRight);
    ~MudMapThread();

private:
    QGraphicsPixmapItem* loadTile(const MudMap::TileSpec &tile);

signals:
    void tileToAdd(QGraphicsItem *tile);
    void tileToRemove(QGraphicsItem *tile);

private:
    QMap<MudMap::TileSpec, QGraphicsItem*> m_tiles;  ///<已加载瓦片图元
    QSet<MudMap::TileSpec>    m_tileSpecSet;         ///<已加载瓦片编号集合
    //
    MudMap::TileSpec m_preTopLeft;
    MudMap::TileSpec m_preBottomRight;
};

#endif // MUDMAP_H
