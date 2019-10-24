#ifndef QMATIO_H
#define QMATIO_H

#include <QIODevice>
#include <QVector>
#include <QSharedDataPointer>

class QMatIOPrivate;
class QMatData;
class QMatVar;
class QMatStruct;

template<class T>
class QMatMatrixData;

class QMatIO
{
public:
	QMatIO();
	QMatIO(QString fileName);
	virtual ~QMatIO();

	bool open(QIODevice::OpenMode flags);
	void close();

	bool write(const QMatStruct &value, bool compressed = false);
	bool write(const QMatVar &value, bool compressed = false);

	QStringList valuesNames() const;
	QList<QMatVar> values() const;
	QMatVar value(QString name) const;
	QMatVar operator()(QString name) const;
	QMatVar operator[](QString name) const;
	QMatVar operator[](size_t index) const;

	QString fileName() const;

	enum Version {
		Unknown = 0,
		Mat4 = 0x0010,
		Mat5 = 0x0100,
		Mat73 = 0x0200
	};

	Version version() const;

	template <typename... Args>
	static void save(QString fileName, QString vars, Args... args) {
		QMatIO *mat = new QMatIO(fileName);
		if (!mat->open(QIODevice::WriteOnly|QIODevice::Truncate))
			return;
		QStringList v = vars.simplified().replace(" ","").split(',');
		QMatIO::savePart(mat, v, args...);
	}

	static QString versionString();
	static void libraryVersion(int &major, int &minor, int &rel);

private:
	QMatIOPrivate * const d_ptr;
	Q_DECLARE_PRIVATE(QMatIO)
	Q_DISABLE_COPY(QMatIO)

	inline static void savePart(QMatIO *p, QStringList) { delete p; }

	template <typename T, typename... Args>
	inline static void savePart(QMatIO *p, QStringList v, T val, Args... args) {
		p->write(QMatVar(val, v.takeFirst()));
		QMatIO::savePart(p, v, args...);
	}

};

template<class T>
class QMatMatrix {
public:
	QMatMatrix();
	QMatMatrix(size_t size1, size_t size2, QString name = QString());
	QMatMatrix(size_t size1, size_t size2, const T *data, QString name = QString());
	QMatMatrix(const QMatMatrix<T> &other);
	virtual ~QMatMatrix();

	size_t size1() const;
	size_t size2() const;

	size_t dims(size_t i) const;

	const T &operator()(size_t i, size_t j) const;
	T &operator()(size_t i, size_t j);

	QMatMatrix<T> &operator =(const QMatMatrix<T> &other);

private:
	QSharedDataPointer<QMatMatrixData<T>> m_var;

	friend class QMatIO;

};

class QMatVar {
public:
	enum Type {
		Empty,
		Cell,
		Struct,
		Object,
		String,
		Double = 6,
		Function = 16,
		StringList = 100
	};

	QMatVar();
	QMatVar(QString string, QString name = QString());
	QMatVar(QStringList list, QString name = QString());
	QMatVar(double value, QString name = QString());
	QMatVar(bool value, QString name = QString());
	QMatVar(size_t dim0, size_t dim1, double *dat, QString name = QString());
	QMatVar(size_t dim0, size_t dim1, float *dat, QString name = QString());
	QMatVar(QVector<double> vec, QString name = QString());
	QMatVar(QVector<float> vec, QString name = QString());
	QMatVar(const QMatVar &other);

	virtual ~QMatVar();

	QMatVar &operator =(const QMatVar &other);

	bool isEmpty() const;

	QString name() const;

	QString typeName() const;
	QString dimString() const;
	QString infoString() const;

	size_t rank() const;
	QVector<size_t> dims() const;
	size_t dims(size_t i) const;

	Type classType() const;

	size_t size() const;

	QMatStruct toStruct() const;

	QStringList toStringList() const;
	QString toString() const;

	QVariant value() const;

	template<typename T>
	T value() const;

	template<typename T>
	QVector<T> toVector() const;

	template<class T>
	QMatMatrix<T> toMatrix(size_t depth = 0) const;

	bool isSingleValue() const;

private:
	enum Alloc { New };
	QMatVar(Alloc alloc);
	QSharedDataPointer<QMatData> m_var;

	friend class QMatStruct;
	friend class QMatIO;
	friend class QMatIOPrivate;

};
Q_DECLARE_METATYPE(QMatVar)

class QMatStruct {
public:
	QMatStruct();
	QMatStruct(const QMatVar &var);
	QMatStruct(QString name, QStringList fieldNames, size_t n = 1);
	QMatStruct(QStringList fieldNames, size_t n = 1);
	QMatStruct(const QMatStruct &other);
	QMatStruct &operator =(const QMatStruct &other);
	virtual ~QMatStruct();

	QString name() const;

	bool isEmpty() const;

	void set(QString fieldName, size_t i, const QMatVar &v);
	void set(size_t field_index, size_t i, const QMatVar &v);
	QMatVar operator()(QString fieldName, size_t i = 0);
	QMatVar operator()(size_t field_index, size_t i = 0);

	size_t fields() const;
	QStringList fieldNames() const;
	QMatVar readField(size_t field_index, size_t index = 0) const;

	QMatVar value(QString fieldName, size_t index = 0) const;

	size_t size() const;
	QVector<size_t> dims() const;
	size_t dims(size_t i) const;

	operator QMatVar() const;

private:
	enum Alloc { New };
	QMatStruct(Alloc alloc, QStringList names);
	QSharedDataPointer<QMatData> m_var;
	QStringList m_fieldNames;

	friend class QMatIO;

};

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<< (QDebug, const QMatVar &);
#endif

#define saveMAT(fileName, ...) QMatIO::save(fileName, #__VA_ARGS__, __VA_ARGS__)

#endif // QMATIO_H
