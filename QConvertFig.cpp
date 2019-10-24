#include "QConvertFig.h"

#include <QFileInfo>
#include <QDebug>
#include <QDir>
#include <QFontMetrics>
#include <QXmlStreamWriter>
#include <QApplication>
#include <QMetaEnum>
#include <QColor>
#include <QPixmap>
#include <QPainter>
#include <QMainWindow>
#include <QToolBar>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QComboBox>
#include <QLineEdit>
#include <QScrollBar>
#include <QCheckBox>
#include <QGroupBox>
#include <QtMath>

struct Action {
	QString name;
	QString text;
	QPixmap icon;
};

struct QConvertFig::Widget {
	inline Widget() : type(Unknown) {}
	inline ~Widget() {
		qDeleteAll(actions);
		qDeleteAll(children);
	}

	enum Type {
		Unknown,
		Axes,
		PushButton,
		ToggleButton,
		Checkbox,
		RadioButton,
		Edit,
		Text,
		Slider,
		ListBox,
		PopupMenu,
		Frame,
		ToolBar
	} type;

	Widget(Type t, QString tag, QString s)
		: type(t), name(tag), styleSheet(s) {}

	QString name;
	QString text;
	QStringList textList;
	QRect geometry;
	QString styleSheet;
	QList<Action*> actions;
	QList<Widget*> children;
};

QConvertFig::QConvertFig(QString fileName)
	: m_height(0)
{
	m_fileName = fileName;
	qDebug() << fileName;
	const QFileInfo fileInfo(fileName);

	QString guiPath = fileInfo.absolutePath();
	QString guiName = fileInfo.baseName();

	if (fileInfo.exists())
		m_outputFile = guiPath +"/" + guiName + "_build.ui";
}

QConvertFig::~QConvertFig()
{
}

bool QConvertFig::convert()
{
	if (m_outputFile.isEmpty()) {
		qDebug() << "Output fileName is empty!";
		return false;
	}

	QMatIO file(m_fileName);
	if (!file.open(QIODevice::ReadOnly)) {
		qDebug() << "Cannot open MAT file!";
		return false;
	}

	QMatStruct var;
	for (QMatVar &n : file.values()) {
		if (n.name().startsWith("hgS_"))
			var = n.toStruct();
	}
	if (var.isEmpty()) {
		qDebug() << "Variable 'hgS_050200' or 'hgS_070000' not found, or error reading MAT file";
		return false;
	}

	QFont font;
	//font = qApp->font();
	font.setFamily(QStringLiteral("MS Sans Serif"));
	//font.setPointSize(10);

	//const size_t fields = var.fields();
	QMatVar type = var.value("type", 0);
	qDebug() << "type" << type.toString();
	QMatStruct properties = var.value("properties", 0).toStruct();
	qDebug() << properties.name() << properties.isEmpty();

	QString windowTitle = properties.value("Name", 0).toString();
	QString menuBar = properties.value("MenuBar", 0).toString();
	QString tag = properties.value("Tag", 0).toString();

	QColor c;
	QMatVar ccolor = properties.value("Color", 0);
	assert(ccolor.dims(0) == 1 || ccolor.dims(1) == 1);
	QVector<double> color = ccolor.toVector<double>();
	c.setRgbF(color[0], color[1], color[2]);

	QMatStruct children = var.value("children", 0).toStruct();
	size_t widgets = children.dims()[0];
	qDebug() << "widgets:" << widgets;

	QRect size = position(properties, font);
	m_height = size.height();

	QList<Widget*> base, other;
	Widget *toolBar = nullptr;
	for (size_t i = 0; i < widgets; i++) {
		Widget *widget = parseWidget(children, i, font);
		if (widget == nullptr) continue;
		if (widget->type == Widget::Frame)
			base.append(widget);
		else if (widget->type == Widget::ToolBar)
			toolBar = widget;
		else
			other.append(widget);
	}

	bool hasIcons = false;
	if (toolBar != nullptr) {
		for (Action *a:toolBar->actions) {
			if (!a->icon.isNull()) {
				hasIcons = true;
				break;
			}
		}
	}

	for (Widget *frame:base) {
		QRect r = frame->geometry;
		for (Widget *widget:other) {
			if ((frame->type == Widget::Frame) && r.contains(widget->geometry)) {
				QRect nr(widget->geometry.topLeft()-r.topLeft(), widget->geometry.size());
				widget->geometry = nr;
				frame->children.append(widget);
				other.removeOne(widget);
			}
		}
	}
	for (Widget *widget:other)
		base.append(widget);
	other.clear();

	QFile output(m_outputFile);
	if (!output.open(QIODevice::WriteOnly))
		return false;
	QXmlStreamWriter stream(&output);
	stream.setAutoFormatting(true);
	stream.setAutoFormattingIndent(1);
	stream.writeStartDocument();

	stream.writeStartElement("ui");
	stream.writeAttribute("version", "4.0");

	stream.writeTextElement("class", "MainWindow");

	stream.writeStartElement("widget");
	stream.writeAttribute("class", "QMainWindow");
	stream.writeAttribute("name", tag);

	writeProperty(stream, "geometry", size);
	writeProperty(stream, "font", font);
	writeProperty(stream, "windowTitle", windowTitle);
	QString styleSheet = QString("#%1 {background-color: rgb(%2, %3, %4); }")
			.arg(tag).arg(c.red()).arg(c.green()).arg(c.blue());
	writeProperty(stream, "styleSheet", styleSheet);

	stream.writeStartElement("widget");
	stream.writeAttribute("class", "QWidget");
	stream.writeAttribute("name", "centralWidget");
	for (Widget *widget:base)
		writeWidget(stream, widget);
	stream.writeEndElement(); // widget centralWidget

	if (menuBar != "none") {
		stream.writeStartElement("widget");
		stream.writeAttribute("class", "QMenuBar");
		stream.writeAttribute("name", "menuBar");
		stream.writeEndElement(); // widget
	}

	if (toolBar != nullptr) {
		stream.writeStartElement("widget");
		stream.writeAttribute("class", "QToolBar");
		stream.writeAttribute("name", "mainToolBar");
		writeAttributeEnum(stream, "toolBarArea", "TopToolBarArea");
		//writeAttribute(stream, "toolBarArea", QVariant::fromValue<Qt::ToolBarAreas>(Qt::TopToolBarArea));
		writeAttribute(stream, "toolBarBreak", false);
		for (Action *a:toolBar->actions) {
			stream.writeStartElement("addaction");
			stream.writeAttribute("name", a->name);
			stream.writeEndElement(); // addaction
		}
		stream.writeEndElement(); // widget

		QString guiPath = QFileInfo(m_outputFile).absolutePath();
		for (Action *a:toolBar->actions) {
			stream.writeStartElement("action");
			stream.writeAttribute("name", a->name);
			a->icon.save(guiPath + "/" + a->name + ".png");

			stream.writeStartElement("property");
			stream.writeAttribute("name", "icon");
			stream.writeStartElement("iconset");
			stream.writeCharacters(a->name + ".png");
			stream.writeEndElement(); // iconset
			stream.writeEndElement(); // property

			writeProperty(stream, "text", a->text);

			stream.writeEndElement(); // action
		}
	}

	stream.writeEndElement(); // widget MainWindow

	stream.writeStartElement("layoutdefault");
	stream.writeAttribute("spacing", "6");
	stream.writeAttribute("margin", "11");
	stream.writeEndElement(); // layoutdefault

	stream.writeStartElement("customwidgets");
	stream.writeStartElement("customwidget");
	stream.writeTextElement("class", "QwtPlot");
	stream.writeTextElement("extends", "QWidget");
	stream.writeStartElement("header");
	stream.writeAttribute("location", "global");
	stream.writeCharacters("qwt_plot.h");
	stream.writeEndElement(); // header
	stream.writeTextElement("container", "1");
	stream.writeEndElement(); // customwidget
	stream.writeEndElement(); // customwidgets

	stream.writeTextElement("pixmapfunction", "");
	stream.writeTextElement("resources", "");
	stream.writeTextElement("connections", "");

	stream.writeEndElement(); // ui

	stream.writeEndDocument();

	output.close();

	qDeleteAll(base);

	return true;
}

QString QConvertFig::outputFileName() const
{
	return m_outputFile;
}

void QConvertFig::writeAttribute(QXmlStreamWriter &xml, QString name, QVariant var) const
{
	xml.writeStartElement("attribute");
	xml.writeAttribute("name", name);
	if (var.type() == QVariant::Bool) {
		xml.writeTextElement("bool", var.toString());
	} else {
#if 0
		int id = QMetaType::type(var.typeName());
		QMetaEnum metaEnum = QMetaEnum::fromType<ModelApple::AppleType>();
		qDebug() << var.canConvert(id) << var.toInt();
		qDebug() << "attribute" << var.type() << var.toString() << var << var.typeName();
#endif
	}
	xml.writeEndElement(); // attribute
}

void QConvertFig::writeAttributeEnum(QXmlStreamWriter &xml, QString name, QString var) const
{
	xml.writeStartElement("attribute");
	xml.writeAttribute("name", name);
	xml.writeTextElement("enum", var);
	xml.writeEndElement(); // attribute
}

void QConvertFig::writeProperty(QXmlStreamWriter &xml, QString name, QVariant var) const
{
	xml.writeStartElement("property");
	xml.writeAttribute("name", name);
	if (var.type() == QVariant::Rect) {
		const QRect rect = var.value<QRect>();
		xml.writeStartElement("rect");
		xml.writeTextElement("x", QString::number(rect.x()));
		xml.writeTextElement("y", QString::number(rect.y()));
		xml.writeTextElement("width", QString::number(rect.width()));
		xml.writeTextElement("height", QString::number(rect.height()));
		xml.writeEndElement(); // rect
	} else if (var.type() == QVariant::String) {
		xml.writeTextElement("string", var.toString());
	} else if (var.type() == QVariant::Font) {
		QFont font = var.value<QFont>();
		xml.writeStartElement("font");
		xml.writeTextElement("family", font.family());
		xml.writeTextElement("pointsize", QString::number(font.pointSize()));
		xml.writeEndElement(); // font
	} else if (var.type() == QVariant::Bool) {
		xml.writeTextElement("bool", var.toString());
	}
	xml.writeEndElement(); // property
}

void QConvertFig::writePropertyEnum(QXmlStreamWriter &xml, QString name, QString className, QString var) const
{
	xml.writeStartElement("property");
	xml.writeAttribute("name", name);
	xml.writeTextElement("enum", className + "::" + var);
	xml.writeEndElement(); // property
}

void QConvertFig::writePropertySet(QXmlStreamWriter &xml, QString name, QString className, QStringList var) const
{
	QStringList use;
	for (QString s:var)
		use.append(className+"::"+s);
	xml.writeStartElement("property");
	xml.writeAttribute("name", name);
	xml.writeTextElement("set", use.join('|'));
	xml.writeEndElement(); // property
}

void QConvertFig::writeWidget(QXmlStreamWriter &xml, Widget *widget) const
{
	if (widget == nullptr) return;
	const QString styleSheet = widget->styleSheet;
	const QString tag = widget->name;
	const QRect r = widget->geometry;
	switch (widget->type) {
	case Widget::Axes:
		xml.writeStartElement("widget");
		//xml.writeAttribute("class", "QwtPlot");
		xml.writeAttribute("class", "QFrame");
		xml.writeAttribute("name", tag);
		xml.writeAttribute("native", "true");
		writeProperty(xml, "geometry", r);
		writePropertyEnum(xml, "frameShape", "QFrame", "StyledPanel");
		writePropertyEnum(xml, "frameShadow", "QFrame", "Sunken");
		if (!styleSheet.isEmpty()) writeProperty(xml, "styleSheet", styleSheet);
		xml.writeEndElement(); // widget
		break;
	case Widget::Frame:
		xml.writeStartElement("widget");
		xml.writeAttribute("class", "QGroupBox");
		xml.writeAttribute("name", tag);
		writeProperty(xml, "geometry", r);
		writeProperty(xml, "title", widget->text);
		if (!styleSheet.isEmpty()) writeProperty(xml, "styleSheet", styleSheet);
		for (Widget *child:widget->children)
			writeWidget(xml, child);
		xml.writeEndElement(); // widget
		break;
	case Widget::PushButton:
	case Widget::ToggleButton:
		xml.writeStartElement("widget");
		xml.writeAttribute("class", "QPushButton");
		xml.writeAttribute("name", tag);
		writeProperty(xml, "geometry", r);
		if (!widget->text.isEmpty())
			writeProperty(xml, "text", widget->text);
		else if (!widget->textList.isEmpty())
			writeProperty(xml, "text", widget->textList.join(" "));
		if (widget->type == Widget::ToggleButton)
			writeProperty(xml, "checkable", true);
		if (!styleSheet.isEmpty()) writeProperty(xml, "styleSheet", styleSheet);
		xml.writeEndElement(); // widget
		break;
	case Widget::Text:
		xml.writeStartElement("widget");
		xml.writeAttribute("class", "QLabel");
		xml.writeAttribute("name", tag);
		writeProperty(xml, "geometry", r);
		if (!widget->text.isEmpty())
			writeProperty(xml, "text", widget->text);
		else if (!widget->textList.isEmpty())
			writeProperty(xml, "text", widget->textList.join(" "));
		if (!styleSheet.isEmpty()) writeProperty(xml, "styleSheet", styleSheet);
		writePropertySet(xml, "alignment", "Qt", QStringList() << "AlignCenter");
		writeProperty(xml, "wordWrap", true);
		xml.writeEndElement(); // widget
		break;
	case Widget::PopupMenu:
	case Widget::ListBox:
		xml.writeStartElement("widget");
		if (widget->type == Widget::PopupMenu)
			xml.writeAttribute("class", "QComboBox");
		else if (widget->type == Widget::ListBox)
			xml.writeAttribute("class", "QListWidget");
		xml.writeAttribute("name", tag);
		writeProperty(xml, "geometry", r);
		for (QString item:widget->textList) {
			xml.writeStartElement("item");
			writeProperty(xml, "text", item);
			xml.writeEndElement(); // widget
		}
		if (!styleSheet.isEmpty()) writeProperty(xml, "styleSheet", styleSheet);
		xml.writeEndElement(); // widget
		break;
	case Widget::Edit:
		xml.writeStartElement("widget");
		xml.writeAttribute("class", "QLineEdit");
		xml.writeAttribute("name", tag);
		writeProperty(xml, "geometry", r);
		if (!widget->text.isEmpty()) writeProperty(xml, "text", widget->text);
		if (!styleSheet.isEmpty())	  writeProperty(xml, "styleSheet", styleSheet);
		xml.writeEndElement(); // widget
		break;
	case Widget::Slider:
		xml.writeStartElement("widget");
		xml.writeAttribute("class", "QScrollBar");
		xml.writeAttribute("name", tag);
		writeProperty(xml, "geometry", r);
		if (r.width() > r.height())
			writePropertyEnum(xml, "orientation", "Qt", "Horizontal");
		else
			writePropertyEnum(xml, "orientation", "Qt", "Vertical");
		if (!styleSheet.isEmpty()) writeProperty(xml, "styleSheet", styleSheet);
		xml.writeEndElement(); // widget
		break;
	case Widget::Checkbox:
	case Widget::RadioButton:
		xml.writeStartElement("widget");
		if (widget->type == Widget::Checkbox)
			xml.writeAttribute("class", "QCheckBox");
		else if (widget->type == Widget::RadioButton)
			xml.writeAttribute("class", "QRadioButton");
		xml.writeAttribute("name", tag);
		writeProperty(xml, "geometry", r);
		writeProperty(xml, "text", widget->text);
		if (!styleSheet.isEmpty()) writeProperty(xml, "styleSheet", styleSheet);
		xml.writeEndElement(); // widget
		break;
	case Widget::ToolBar:
		break;
	case Widget::Unknown:
		qDebug() << "writeWidget: unknown type" << widget->type;
		break;
	}
}

QRect QConvertFig::position(const QMatStruct &properties, const QFont &font) const
{
	double unitH = 1.0, unitV = 1.0;
	QString units = properties.value("Units", 0).toString();
	if (units == "characters") {
		/* These units are based on the default uicontrol font of the graphics root object:
		 * Character width = width of the letter x.
		 * Character height = distance between the baselines of two lines of text.
		 * To access the default uicontrol font, use get(groot,'defaultuicontrolFontName')
		 * or set(groot,'defaultuicontrolFontName').
		 */
		unitH = QFontMetrics(font).horizontalAdvance("x");
		unitV = QFontMetrics(font).lineSpacing();
	} else if (units == "pixels") {
		/* Pixels.
		 * Starting in R2015b, distances in pixels are independent of your system resolution on Windows and Macintosh systems:
		 * On Windows systems, a pixel is 1/96th of an inch.
		 * On Macintosh systems, a pixel is 1/72nd of an inch.
		 * On Linux systems, the size of a pixel is determined by your system resolution.
		 */
		unitH = unitV = 1.0;
	} else if (units == "inches") {
		unitH = unitV = 96.0;
	} else if (units == "centimeters") {
		unitH = unitV = 243.84;
	} else if (units == "points") {
		/* Points.
		 * One point equals 1/72nd of an inch.
		 */
		unitH = unitV = 96.0/72.0;
	}

	QMatVar positon = properties.value("Position", 0);
	assert(positon.dims(0) == 1 || positon.dims(1) == 1);

	QVector<double> pos = positon.toVector<double>();
	//qDebug() << pos << units;
	// [left bottom width height]
	return QRect(qCeil(pos[0]*unitH), qCeil(qAbs(m_height - (pos[1] + pos[3])*unitV)),
					 qCeil(pos[2]*unitH), qCeil(pos[3]*unitV));
}

QPixmap QConvertFig::cdataToPixmap(const QMatVar &var) const
{
	QPixmap pixmap(static_cast<int>(var.dims()[1]), static_cast<int>(var.dims()[0]));
	pixmap.fill(Qt::transparent);
	QMatMatrix<double> cdataRed = var.toMatrix<double>(0);
	QMatMatrix<double> cdataGreen = var.toMatrix<double>(1);
	QMatMatrix<double> cdataBlue = var.toMatrix<double>(2);
	QPainter painter;
	painter.begin(&pixmap);
	QColor c;
	for (size_t x = 0; x < var.dims()[1]; x++) {
		for (size_t y = 0; y < var.dims()[0]; y++) {
			const double r = cdataRed(y,x);
			const double g = cdataGreen(y,x);
			const double b = cdataBlue(y,x);
			if (qIsNaN(r) || qIsNaN(g) || qIsNaN(b))
				continue;
			c.setRgbF(r, g, b);
			painter.setPen(c);
			painter.drawPoint(static_cast<int>(x), static_cast<int>(y));
		}
	}
	painter.end();
	return pixmap;
}

QConvertFig::Widget *QConvertFig::parseWidget(QMatStruct &var, size_t i, const QFont &font)
{
	QString type = var.value("type", i).toString();
	QMatStruct props = var.value("properties", i).toStruct();
	if (props.isEmpty())
		return nullptr;
	QString tag = props.value("Tag", 0).toString();
	QMatVar bgColor = props.value("BackgroundColor", 0);
	QString styleSheet;
	QMatVar string = props.value("String", 0);

	if (!bgColor.isEmpty()) {
		QColor c;
		assert(bgColor.dims(0) == 1 || bgColor.dims(1) == 1);
		QVector<double> color = bgColor.toVector<double>();
		c.setRgbF(color[0], color[1], color[2]);
		styleSheet = QString("#%1 {background-color: rgb(%2, %3, %4); }")
				.arg(tag).arg(c.red()).arg(c.green()).arg(c.blue());
	}

	Widget *widget = nullptr;
	if (type == "axes") {
		widget = new Widget(Widget::Axes, tag, styleSheet);
		widget->geometry = position(props, font);
	} else if (type == "uicontrol") {
		QString style = props.value("Style", 0).toString();
		if (style.isEmpty()) {
			widget = new Widget(Widget::PushButton, tag, styleSheet);
			widget->geometry = position(props, font);
			widget->text = string.toString();
		} else if (style == "text") {
			widget = new Widget(Widget::Text, tag, styleSheet);
			widget->geometry = position(props, font);
			if (string.classType() == QMatVar::String)
				widget->text = string.toString();
			else if (string.classType() == QMatVar::StringList)
				widget->textList = string.toStringList();
		} else if (style == "popupmenu") {
			widget = new Widget;
			widget = new Widget(Widget::PopupMenu, tag, styleSheet);
			widget->geometry = position(props, font);
			widget->textList = string.toStringList();
		} else if (style == "edit") {
			widget = new Widget(Widget::Edit, tag, styleSheet);
			widget->geometry = position(props, font);
			if (string.classType() == QMatVar::String)
				widget->text = string.toString();
			else if (string.classType() == QMatVar::StringList)
				widget->textList = string.toStringList();
		} else if (style == "slider") {
			widget = new Widget;
			widget = new Widget(Widget::Slider, tag, styleSheet);
			widget->geometry = position(props, font);
		} else if (style == "checkbox") {
			widget = new Widget(Widget::Checkbox, tag, styleSheet);
			widget->geometry = position(props, font);
			if (string.classType() == QMatVar::String)
				widget->text = string.toString();
			else if (string.classType() == QMatVar::StringList)
				widget->textList = string.toStringList();
		} else {
			qDebug() << "parseWidget: unknown style:" << style;
			return nullptr;
		}
	} else if (type == "uipanel") {
		widget = new Widget(Widget::Frame, tag, styleSheet);
		QRect r = position(props, font);
		widget->geometry = r;
		widget->text = props.value("Title", 0).toString();
		auto childs = var.value("children", i).toStruct();
		size_t count = childs.size();
		int height = m_height;
		m_height = r.height();
		for (size_t j = 0; j < count; j++) {
			Widget *w = parseWidget(childs, j, font);
			if (w == nullptr) continue;
			widget->children.append(w);
		}
		m_height = height;
	} else if (type == "uitoolbar") {
		widget = new Widget(Widget::ToolBar, tag, styleSheet);
		auto childs = var.value("children", i).toStruct();
		size_t count = childs.fields();
		for (size_t j = 0; j < count; j++) {
			type = childs.value("type", j).toString();
			if (type != "uitoggletool") continue;
			props = childs.value("properties", j);
			QString tag = props.value("Tag", 0).toString();
			QString toolTip = props.value("TooltipString", 0).toString();
			QMatVar cdata = props.value("CData", 0);
			QPixmap icon = cdataToPixmap(cdata);
			Action *action = new Action;
			action->name = tag;
			action->text = toolTip;
			action->icon = icon;
			widget->actions.append(action);
		}
	}
	if (widget == nullptr)
		qDebug() << "parseWidget: unknown type:" << type;
	return widget;
}
