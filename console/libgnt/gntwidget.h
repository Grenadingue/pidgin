#ifndef GNT_WIDGET_H
#define GNT_WIDGET_H

#include <stdio.h>
#include <glib.h>
#include <glib-object.h>
#include <ncursesw/curses.h>

#define GNT_TYPE_WIDGET				(gnt_widget_get_gtype())
#define GNT_WIDGET(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), GNT_TYPE_WIDGET, GntWidget))
#define GNT_WIDGET_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), GNT_TYPE_WIDGET, GntWidgetClass))
#define GNT_IS_WIDGET(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), GNT_TYPE_WIDGET))
#define GNT_IS_OBJECT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), GNT_TYPE_WIDGET))
#define GNT_WIDGET_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GNT_TYPE_WIDGET, GntWidgetClass))

#define GNT_WIDGET_FLAGS(obj)				(GNT_WIDGET(obj)->priv.flags)
#define GNT_WIDGET_SET_FLAGS(obj, flags)		(GNT_WIDGET_FLAGS(obj) |= flags)
#define GNT_WIDGET_UNSET_FLAGS(obj, flags)	(GNT_WIDGET_FLAGS(obj) &= ~(flags))
#define GNT_WIDGET_IS_FLAG_SET(obj, flags)	(GNT_WIDGET_FLAGS(obj) & (flags))
#define	DEBUG
//#define	DEBUG	printf("%s\n", __FUNCTION__)

typedef struct _GnWidget			GntWidget;
typedef struct _GnWidgetPriv		GntWidgetPriv;
typedef struct _GnWidgetClass		GntWidgetClass;

#define	N_(X)	X

typedef enum _GnWidgetFlags
{
	GNT_WIDGET_DESTROYING	= 1 << 0,
	GNT_WIDGET_CAN_TAKE_FOCUS= 1 << 1,
	GNT_WIDGET_MAPPED 		= 1 << 2,
	/* XXX: Need to set the following two as properties, and setup a callback whenever these
	 * get chnaged. */
	GNT_WIDGET_NO_BORDER		= 1 << 3,
	GNT_WIDGET_NO_SHADOW		= 1 << 4,
	GNT_WIDGET_HAS_FOCUS		= 1 << 5
} GntWidgetFlags;

typedef enum _GnParamFlags
{
	GNT_PARAM_SERIALIZABLE	= 1 << G_PARAM_USER_SHIFT
} GntParamFlags;

struct _GnWidgetPriv
{
	int x, y;
	int width, height;
	GntWidgetFlags flags;
	char *name;
};

struct _GnWidget
{
	GObject inherit;

	GntWidget *parent;

	GntWidgetPriv priv;
	WINDOW *window;
	WINDOW *back;

    void (*gnt_reserved1)(void);
    void (*gnt_reserved2)(void);
    void (*gnt_reserved3)(void);
    void (*gnt_reserved4)(void);
};

struct _GnWidgetClass
{
	GObjectClass parent;

	void (*map)(GntWidget *obj);
	void (*show)(GntWidget *obj);		/* This will call draw() and take focus (if it can take focus) */
	void (*destroy)(GntWidget *obj);
	void (*draw)(GntWidget *obj);		/* This will draw the widget */
	void (*expose)(GntWidget *widget, int x, int y, int width, int height);
	void (*gained_focus)(GntWidget *widget);
	void (*lost_focus)(GntWidget *widget);

	void (*size_request)(GntWidget *widget);
	void (*set_position)(GntWidget *widget, int x, int y);
	gboolean (*key_pressed)(GntWidget *widget, const char *key);
	void (*activate)(GntWidget *widget);

	void (*gnt_reserved1)(void);
	void (*gnt_reserved2)(void);
	void (*gnt_reserved3)(void);
	void (*gnt_reserved4)(void);
};

G_BEGIN_DECLS

GType gnt_widget_get_gtype(void);
void gnt_widget_destroy(GntWidget *widget);
void gnt_widget_show(GntWidget *widget);
void gnt_widget_draw(GntWidget *widget);
void gnt_widget_expose(GntWidget *widget, int x, int y, int width, int height);
void gnt_widget_hide(GntWidget *widget);

void gnt_widget_get_position(GntWidget *widget, int *x, int *y);
void gnt_widget_set_position(GntWidget *widget, int x, int y);
void gnt_widget_size_request(GntWidget *widget);
void gnt_widget_get_size(GntWidget *widget, int *width, int *height);
void gnt_widget_set_size(GntWidget *widget, int width, int height);

gboolean gnt_widget_key_pressed(GntWidget *widget, const char *keys);

gboolean gnt_widget_set_focus(GntWidget *widget, gboolean set);
void gnt_widget_activate(GntWidget *widget);

void gnt_widget_set_name(GntWidget *widget, const char *name);

G_END_DECLS

#endif /* GNT_WIDGET_H */