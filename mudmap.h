#ifndef MUDMAP_H
#define MUDMAP_H

#include <QWidget>
#include <QGraphicsView>
#include <QtLocation/private/qgeoprojection_p.h>
#include <QWheelEvent>

class GraphicsView : public QGraphicsView
{
    Q_OBJECT
public:
    struct TileSpec {
        int zoom;
        int x;
        int y;
        bool operator <(const TileSpec &rhs) const;
    };
    GraphicsView(QGraphicsScene *scene);

    QGraphicsPixmapItem* addTile(const TileSpec &tile);

protected:
    void wheelEvent(QWheelEvent *e) override;
private:
    void fitTile();
private:
    QMap<TileSpec, QGraphicsPixmapItem*> m_tiles;
};
/*!
 * \brief 基于Graphics View的地图
 * \details 其仅用于显示瓦片地图
 */
class MudMap : public QWidget
{
    Q_OBJECT

public:
    MudMap(QWidget *parent = nullptr);
    ~MudMap();

protected:

private:
    QGraphicsView  *m_view;
    QGraphicsScene *m_scene;
};
#endif // MUDMAP_H
