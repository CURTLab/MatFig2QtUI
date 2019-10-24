#include "QMatIO.h"

extern "C" {
#include <matio.h>
}

#include <QFileInfo>
#include <QDebug>
#include <QDir>
#include <QSharedData>
#include <QTemporaryFile>
#include <assert.h>
#include <algorithm>

// private class declaration
class QMatIOPrivate
{
	QMatIO * const q_ptr;
	Q_DECLARE_PUBLIC(QMatIO)

public:
	inline  QMatIOPrivate(QMatIO *parent)
		: q_ptr(parent), mat(nullptr), tf(nullptr) {}

	inline  QMatIOPrivate(QString fileName, QMatIO *parent)
		: q_ptr(parent), mat(nullptr), fileName(fileName), tf(nullptr) {}

	inline  ~QMatIOPrivate() { delete tf; }

	mat_t *mat;
	QString fileName;
	QList<QMatVar> values;
	QTemporaryFile *tf;

	static mat_t *createMatFile(QString fileName);

	static matvar_t *createDouble(QString name, const std::vector<size_t> &dims, void *data);
	static matvar_t *createStruct(QString name, QStringList fieldNames, size_t n = 1);
	static matvar_t *createString(QString name, QString string);
	static matvar_t *createStringList(QString name, QStringList list);
	static matvar_t *createBool(QString name, bool val);

	template <class T>
	static matvar_t *createArray(QString name, const std::vector<size_t> &dims, T *data);

	static QMatVar create(matvar_t *d);

	enum MemoryOrder {
		RowMajor,
		ColumnMajor
	};

	static constexpr MemoryOrder order = RowMajor;

	inline static void savePart(QMatIOPrivate *p, QStringList) {
		delete p;
	}
};

class QMatData : public QSharedData
{
public:
	inline QMatData() : d(nullptr) {}
	inline QMatData(matvar_t *data) : d(data) {}
	//~QMatData() { if (d) Mat_VarFree(d); }
	matvar_t *d;

	inline operator matvar_t *() { return d; }

	friend class QMatIO;
};

struct Helper {
	template<typename T> static matio_classes matClass();
	template<typename T> static matio_types matType();
};

// supported by QMatrix (gsl)
template<> matio_classes Helper::matClass<double>() { return MAT_C_DOUBLE; }
template<> matio_classes Helper::matClass<float>() { return MAT_C_SINGLE; }
template<> matio_classes Helper::matClass<int>() { return MAT_C_INT32; }
template<> matio_types Helper::matType<double>() { return MAT_T_DOUBLE; }
template<> matio_types Helper::matType<float>() { return MAT_T_SINGLE; }
template<> matio_types Helper::matType<int>() { return MAT_T_INT32; }

QMatIO::QMatIO()
	: d_ptr(new QMatIOPrivate(this))
{
}

QMatIO::QMatIO(QString fileName)
	: d_ptr(new QMatIOPrivate(fileName, this))
{
}

QMatIO::~QMatIO()
{
	Q_D(QMatIO);
	close();
	delete d;
}

bool QMatIO::open(QIODevice::OpenMode flags)
{
	Q_D(QMatIO);
	if (flags.testFlag(QIODevice::WriteOnly)) {
		if (flags.testFlag(QIODevice::Truncate))
			QFile(d->fileName).remove();
		if ((d->mat == nullptr) && QFile(d->fileName).exists()) {
			d->mat = Mat_Open(qPrintable(QDir::toNativeSeparators(d->fileName)), MAT_ACC_RDWR);
			return (d->mat != nullptr);
		}
		d->mat = QMatIOPrivate::createMatFile(d->fileName);
	} else if (flags.testFlag(QIODevice::ReadOnly)) {
		QString fileName = d->fileName;
		if (fileName.startsWith(":") || fileName.startsWith("qrc:")) {
			QFile f(fileName);
			d->tf = QTemporaryFile::createNativeFile(f);
			fileName = d->tf->fileName();
		}
		d->mat = Mat_Open(qPrintable(QDir::toNativeSeparators(fileName)), MAT_ACC_RDONLY);
	}
	return (d->mat != nullptr);
}

void QMatIO::close()
{
	Q_D(QMatIO);
	if (d->mat) {
		Mat_Close(d->mat);
		d->mat = nullptr;
	}
}

mat_t *QMatIOPrivate::createMatFile(QString fileName)
{
	return Mat_CreateVer(qPrintable(QDir::toNativeSeparators(fileName)), nullptr, MAT_FT_MAT5);
}

matvar_t *QMatIOPrivate::createDouble(QString name, const std::vector<size_t> &dims, void *data)
{
	return Mat_VarCreate(qPrintable(name), MAT_C_DOUBLE, MAT_T_DOUBLE, static_cast<int>(dims.size()), const_cast<size_t *>(dims.data()), data, 0);
}

matvar_t *QMatIOPrivate::createStruct(QString name, QStringList fieldNames, size_t n)
{
	size_t dims[2] = {1, n};

	const int N = fieldNames.size();
	char **fieldnames = reinterpret_cast<char **>(malloc(static_cast<size_t>(N) * sizeof(char *)));
	for (int i = 0; i < N; i++) {
		const size_t size = static_cast<size_t>(fieldNames[i].length()+1)*sizeof(char);
		char *datastr = reinterpret_cast<char *>(malloc(size));
		strcpy_s(datastr, size, qPrintable(fieldNames[i]));
		datastr[fieldNames[i].length()] = '\0';
		fieldnames[i] = datastr;
	}
	return Mat_VarCreateStruct(qPrintable(name), 2, dims, const_cast<const char**>(fieldnames), static_cast<unsigned>(N));
}

matvar_t *QMatIOPrivate::createString(QString name, QString string)
{
	size_t dims[2] = {1, 1};
	dims[1] = static_cast<size_t>(string.length());
	const size_t size = (dims[1]+1)*sizeof(char);
	char *datastr = reinterpret_cast<char *>(malloc(size));
	strcpy_s(datastr, size, qPrintable(string));
	datastr[string.length()] = '\0';
	return Mat_VarCreate(!name.isEmpty()?qPrintable(name):nullptr, MAT_C_CHAR, MAT_T_UTF8, 2, dims, datastr, 0);
}

matvar_t *QMatIOPrivate::createStringList(QString name, QStringList list)
{
	const int N = list.size();
	size_t dims[2] = {static_cast<size_t>(N), 1};

	matvar_t **matvar = reinterpret_cast<matvar_t **>(malloc(static_cast<size_t>(N) * sizeof(matvar_t *)));
	for (int i = 0; i < N; i++)
		matvar[i] = createString("data", list[i]);
	return Mat_VarCreate(qPrintable(name), MAT_C_CELL, MAT_T_CELL, 2, dims, matvar, 0);
}

matvar_t *QMatIOPrivate::createBool(QString name, bool state)
{
	size_t dims[2] = {1, 1};
	mat_uint8_t val = static_cast<mat_uint8_t>(state);
	return Mat_VarCreate(qPrintable(name),MAT_C_UINT8,MAT_T_UINT8,2,dims,&val,MAT_F_LOGICAL);
}

template<>
matvar_t *QMatIOPrivate::createArray(QString name, const std::vector<size_t> &dims, double *data)
{
	return Mat_VarCreate(qPrintable(name), MAT_C_DOUBLE, MAT_T_DOUBLE, static_cast<int>(dims.size()), const_cast<size_t *>(dims.data()), reinterpret_cast<void*>(data), 0);
}

template<>
matvar_t *QMatIOPrivate::createArray(QString name, const std::vector<size_t> &dims, float *data)
{
	return Mat_VarCreate(qPrintable(name), MAT_C_SINGLE, MAT_T_SINGLE, static_cast<int>(dims.size()), const_cast<size_t *>(dims.data()), reinterpret_cast<void*>(data), 0);
}

QMatVar QMatIOPrivate::create(matvar_t *d)
{
	QMatVar var(QMatVar::New);
	var.m_var->d = d;
	return var;
}

bool QMatIO::write(const QMatStruct &value, bool compressed)
{
	Q_D(QMatIO);
	if (d->mat == nullptr)
		return false;
	return (Mat_VarWrite(d->mat, value.m_var->d, compressed?MAT_COMPRESSION_ZLIB:MAT_COMPRESSION_NONE) == 0);
}

bool QMatIO::write(const QMatVar &value, bool compressed)
{
	Q_D(QMatIO);
	if (d->mat == nullptr)
		return false;
	return (Mat_VarWrite(d->mat, value.m_var->d, compressed?MAT_COMPRESSION_ZLIB:MAT_COMPRESSION_NONE) == 0);
}

QStringList QMatIO::valuesNames() const
{
	const Q_D(QMatIO);
	QStringList ret;
	if (d->mat != nullptr) {
		Mat_Rewind(d->mat);
		matvar_t *var = nullptr;
		while ((var = Mat_VarReadNext(d->mat)) != nullptr)
			ret << QString(var->name);
	}
	return ret;
}

QList<QMatVar> QMatIO::values() const
{
	const Q_D(QMatIO);
	QList<QMatVar> ret;
	if (d->mat != nullptr) {
		Mat_Rewind(d->mat);
		matvar_t *var = nullptr;
		while ((var = Mat_VarReadNext(d->mat)) != nullptr)
			ret << QMatIOPrivate::create(var);
	}
	return ret;
}

QMatVar QMatIO::value(QString name) const
{
	const Q_D(QMatIO);
	if (d->mat == nullptr)
		return QMatVar();
	return QMatIOPrivate::create(Mat_VarRead(d->mat, qPrintable(name)));
}

QMatVar QMatIO::operator()(QString name) const { return value(name); }
QMatVar QMatIO::operator[](QString name) const { return value(name); }

QMatVar QMatIO::operator[](size_t index) const
{
	const Q_D(QMatIO);
	if (d->mat != nullptr) {
		Mat_Rewind(d->mat);
		matvar_t *var = nullptr;
		for (size_t i = 0; i < index; ++i) {
			var = Mat_VarReadNext(d->mat);
			if (var == nullptr)
				return QMatVar();
		}
		return QMatIOPrivate::create(var);
	}
	return QMatVar();
}

QString QMatIO::fileName() const
{
	const Q_D(QMatIO);
	return d->fileName;
}

QMatIO::Version QMatIO::version() const
{
	const Q_D(QMatIO);
	return static_cast<Version>(Mat_GetVersion(d->mat));
}

QString QMatIO::versionString()
{
	int major, minor, rel;
	Mat_GetLibraryVersion(&major, &minor, &rel);
	return QString("libmatio %1.%2.%3").arg(major).arg(minor).arg(rel);
}

void QMatIO::libraryVersion(int &major, int &minor, int &rel)
{
	Mat_GetLibraryVersion(&major, &minor, &rel);
}

template<class T>
class QMatMatrixData : public QSharedData
{
public:
	inline QMatMatrixData(size_t n1, size_t n2, QString name)
		: n1(n1), n2(n2), name(name), data(new T[n1*n2]) {}
	inline ~QMatMatrixData() { delete [] data; }

	size_t n1;
	size_t n2;
	QString name;
	T *data;

};

template<class T>
QMatMatrix<T>::QMatMatrix()
{
}

template<class T>
QMatMatrix<T>::QMatMatrix(size_t size1, size_t size2, QString name)
	: m_var(new QMatMatrixData<T>(size1, size2, name))
{
}

template<class T>
QMatMatrix<T>::QMatMatrix(size_t size1, size_t size2, const T *data, QString name)
	: m_var(new QMatMatrixData<T>(size1, size2, name))
{
	memcpy(m_var->data, data, size1 * size2 * sizeof(T));
}

template<class T>
QMatMatrix<T>::QMatMatrix(const QMatMatrix<T> &other)
	: m_var(other.m_var)
{
}

template<class T>
QMatMatrix<T>::~QMatMatrix()
{
}

template<class T>
size_t QMatMatrix<T>::size1() const
{
	return m_var ? m_var->n1 : 0;
}

template<class T>
size_t QMatMatrix<T>::size2() const
{
	return m_var ? m_var->n2 : 0;
}

template<class T>
size_t QMatMatrix<T>::dims(size_t i) const
{
	if (!m_var) return 0;
	else if (i == 0) return m_var->n1;
	else if (i == 1) return m_var->n2;
}

template<class T>
const T &QMatMatrix<T>::operator()(size_t i, size_t j) const
{
	assert(m_var);
	if constexpr (QMatIOPrivate::order == QMatIOPrivate::RowMajor)
		return m_var->data[i + j * m_var->n1];
	else if constexpr (QMatIOPrivate::order == QMatIOPrivate::ColumnMajor)
		return m_var->data[i * m_var->n1 + j];
}

template<class T>
T &QMatMatrix<T>::operator()(size_t i, size_t j)
{
	assert(m_var);
	if constexpr (QMatIOPrivate::order == QMatIOPrivate::RowMajor)
		return m_var->data[i + j * m_var->n1];
	else if constexpr (QMatIOPrivate::order == QMatIOPrivate::ColumnMajor)
		return m_var->data[i * m_var->n1 + j];
}

template<class T>
QMatMatrix<T> &QMatMatrix<T>::operator=(const QMatMatrix<T> &other)
{
	m_var = other.m_var;
	return  *this;
}

template class QMatMatrix<double>;
template class QMatMatrix<float>;
template class QMatMatrix<int>;

QMatVar::QMatVar() {}

QMatVar::QMatVar(QMatVar::Alloc /*alloc*/) : m_var(new QMatData) {}

QMatVar::QMatVar(QString string, QString name) : QMatVar(New)
{
	m_var->d = QMatIOPrivate::createString(qPrintable(name), string);
}

QMatVar::QMatVar(QStringList list, QString name) : QMatVar(New)
{
	m_var->d = QMatIOPrivate::createStringList(qPrintable(name), list);
}

QMatVar::QMatVar(double value, QString name) : QMatVar(New)
{
	double *d = new double[1];
	d[0] = value;
	std::vector<size_t> dims = {1};
	m_var->d = QMatIOPrivate::createDouble(name, dims, d);
}

QMatVar::QMatVar(bool value, QString name) : QMatVar(New)
{
	m_var->d = QMatIOPrivate::createBool(name, value);
}

QMatVar::QMatVar(size_t dim0, size_t dim1, double *dat, QString name) : QMatVar(New)
{
	std::vector<size_t> dim({dim0,dim1});
	m_var->d = QMatIOPrivate::createArray<double>(name, dim, dat);
}

QMatVar::QMatVar(size_t dim0, size_t dim1, float *dat, QString name) : QMatVar(New)
{
	std::vector<size_t> dim({dim0,dim1});
	m_var->d = QMatIOPrivate::createArray<float>(name, dim, dat);
}

QMatVar::QMatVar(QVector<double> vec, QString name) : QMatVar(New)
{
	std::vector<size_t> dim({static_cast<size_t>(vec.size())});
	m_var->d = QMatIOPrivate::createArray<double>(name, dim, vec.data());
}

QMatVar::QMatVar(QVector<float> vec, QString name) : QMatVar(New)
{
	std::vector<size_t> dim({static_cast<size_t>(vec.size())});
	m_var->d = QMatIOPrivate::createArray<float>(name, dim, vec.data());
}

QMatVar::QMatVar(const QMatVar &other)
	: m_var(other.m_var)
{
}

QMatVar &QMatVar::operator =(const QMatVar &other)
{
	m_var = other.m_var;
	return *this;

}

QMatVar::~QMatVar()
{
}

#ifdef USE_GSL
QMatVar::QMatVar(const gsl::matrix &m, const QString &name) : QMatVar(New)
{
	size_t dim[2] = {m.size1(), m.size2()};
#if 0
	const double *data = ((const gsl_matrix*)m)->data;
#else
	const gsl::matrix mt = m.transpose();
	const double *data = mt.operator const gsl_matrix *()->data;
#endif
	matvar_t *var = Mat_VarCreate(qPrintable(name), MAT_C_DOUBLE, MAT_T_DOUBLE,
											2, dim, const_cast<double*>(data), 0);
	m_var->d = var;
}
#endif

bool QMatVar::isEmpty() const
{
	return (!m_var->d || (m_var.data() == nullptr));
}

QString QMatVar::name() const
{
	return QString(m_var->d->name);
}

QString QMatVar::typeName() const
{
	switch (m_var->d->class_type) {
	case MAT_C_EMPTY: return "empty";
	case MAT_C_CELL: return "cell";
	case MAT_C_STRUCT: return "struct";
	case MAT_C_OBJECT: return "object";
	case MAT_C_CHAR: return "char";
	case MAT_C_SPARSE: return "sparse";
	case MAT_C_DOUBLE: return "double";
	case MAT_C_SINGLE: return "single";
	case MAT_C_INT8: return "int8";
	case MAT_C_UINT8: return "uint8";
	case MAT_C_INT16: return "int16";
	case MAT_C_UINT16: return "uint16";
	case MAT_C_INT32: return "int32";
	case MAT_C_UINT32: return "uint32";
	case MAT_C_INT64: return "int64";
	case MAT_C_UINT64: return "uint64";
	case MAT_C_FUNCTION: return "function";
	case MAT_C_OPAQUE: return "opaque";
	}

	return "unknown";
}

QString QMatVar::dimString() const
{
	QStringList ret;
	for (int i = 0; i < m_var->d->rank; i++)
		ret << QString::number(m_var->d->dims[i]);
	return ret.join('x');
}

QString QMatVar::infoString() const
{
	return QString("<%1 %2>").arg(dimString()).arg(typeName());
}

size_t QMatVar::rank() const
{
	return static_cast<size_t>(m_var->d->rank);
}

QVector<size_t> QMatVar::dims() const
{
	QVector<size_t> ret(m_var->d->rank);
	for (int i = 0; i < m_var->d->rank; i++)
		ret[i] = m_var->d->dims[i];
	return ret;
}

size_t QMatVar::dims(size_t i) const
{
	assert(i < static_cast<size_t>(m_var->d->rank));
	return m_var->d->dims[i];
}

QMatVar::Type QMatVar::classType() const
{
	if ((m_var->d->class_type == MAT_C_CELL) && (Mat_VarGetCell(m_var->d, 0)->class_type == MAT_C_CELL))
		return StringList;
	return static_cast<Type>(m_var->d->class_type);
}

size_t QMatVar::size() const
{
	return m_var->d->nbytes / static_cast<size_t>(m_var->d->data_size);
}

QStringList QMatVar::toStringList() const
{
	if (m_var->d->class_type != MAT_C_CELL)
		return QStringList();
	size_t n = m_var->d->dims[0];
	QStringList values;
	for (size_t i = 0; i < n; i++) {
		matvar_t *var = Mat_VarGetCell(m_var->d, static_cast<int>(i));
		if (var->class_type == MAT_C_CHAR)
			values.append(QString::fromLatin1(reinterpret_cast<const char*>(var->data), static_cast<int>(var->nbytes) / var->data_size));
	}
	return values;
}

QString QMatVar::toString() const
{
	if (!m_var->d || (m_var->d->class_type != MAT_C_CHAR))
		return QString();
	return QString::fromLatin1(reinterpret_cast<const char*>(m_var->d->data));
}

QVariant QMatVar::value() const
{
	const matvar_t *var = m_var->d;
	if (!var)
		return QVariant();
	if (var->class_type == MAT_C_CHAR) {
		return QVariant(toString());
	} else if (var->class_type == MAT_C_FUNCTION) {
		//Mat_VarPrint(m_var->d, 1);
	} else if (var->class_type == MAT_C_CELL) {
		return toStringList();
	}
	return QVariant();
}

template<class T>
QMatMatrix<T> QMatVar::toMatrix(size_t depth) const
{
	if ((m_var->d->rank < 2) || (m_var->d->data_type != Helper::matType<T>()))
		return QMatMatrix<T>();
	size_t n1 = m_var->d->dims[0];
	size_t n2 = m_var->d->dims[1];
	if ((m_var->d->rank > 2) && (depth < m_var->d->dims[2])) {
		const T *data = reinterpret_cast<T*>(m_var->d->data);
		return QMatMatrix<T>(n1, n2, &data[depth*(n1*n2)]);
	} else {
		return QMatMatrix<T>(n1, n2, reinterpret_cast<T*>(m_var->d->data));
	}
	return QMatMatrix<T>();
}

template<> QString QMatVar::value() const { return toString(); }
template<> QStringList QMatVar::value() const { return toStringList(); }
template<> double *QMatVar::value() const {
	if (m_var->d && m_var->d->class_type == MAT_C_DOUBLE)
		return reinterpret_cast<double *>(m_var->d->data);
	return nullptr;
}
template<> double QMatVar::value() const {
	if (m_var->d && m_var->d->class_type == MAT_C_DOUBLE)
		return reinterpret_cast<double *>(m_var->d->data)[0];
	return 0.0;
}

bool QMatVar::isSingleValue() const
{
	if (size() == 1)
		return true;
	return false;
}

QMatStruct QMatVar::toStruct() const
{
	return QMatStruct(*this);
}

QMatStruct::QMatStruct(QMatStruct::Alloc, QStringList names) : m_var(new QMatData), m_fieldNames(names) {}

QMatStruct::QMatStruct() {}

QMatStruct::QMatStruct(const QMatVar &var) : QMatStruct(New,QStringList())
{
	if (var.m_var->d && var.m_var.data() && (var.classType() == QMatVar::Struct)) {
		m_var->d = Mat_VarDuplicate(var.m_var->d, 0); // deep-copy
		char * const * names = Mat_VarGetStructFieldnames(m_var->d);
		if(names != nullptr) {
			for (size_t i = 0; i < fields(); i++)
				m_fieldNames << QString(names[i]);
		}
	}
}

QMatStruct::QMatStruct(QString name, QStringList fieldNames, size_t n)
	: QMatStruct(New, fieldNames)
{
	m_var->d = QMatIOPrivate::createStruct(name, fieldNames, n);
}

QMatStruct::QMatStruct(QStringList fieldNames, size_t n)
	: QMatStruct("", fieldNames, n)
{
}

QMatStruct::QMatStruct(const QMatStruct &other)
	: m_var(other.m_var)
{
}

QMatStruct &QMatStruct::operator =(const QMatStruct &other)
{
	m_var = other.m_var;
	return *this;
}

QMatStruct::~QMatStruct()
{
}

QString QMatStruct::name() const
{
	return QString(m_var->d->name);
}

bool QMatStruct::isEmpty() const
{
	return (!m_var->d || (m_var.data() == nullptr));
}

void QMatStruct::set(QString fieldName, size_t i, const QMatVar &v)
{
	Mat_VarSetStructFieldByName(m_var->d, qPrintable(fieldName), i, v.m_var->d);
}

void QMatStruct::set(size_t field_index, size_t i, const QMatVar &v)
{
	Mat_VarSetStructFieldByIndex(m_var->d, field_index, i, v.m_var->d);
}

QMatVar QMatStruct::operator()(QString fieldName, size_t i)
{
	return QMatIOPrivate::create(Mat_VarGetStructFieldByName(m_var->d, qPrintable(fieldName), i));
}

QMatVar QMatStruct::operator()(size_t field_index, size_t i)
{
	return QMatIOPrivate::create(Mat_VarGetStructFieldByIndex(m_var->d, field_index, i));
}

size_t QMatStruct::fields() const
{
	return Mat_VarGetNumberOfFields(m_var->d);
}

QStringList QMatStruct::fieldNames() const
{
	return m_fieldNames;
}

QMatVar QMatStruct::readField(size_t field_index, size_t index) const
{
	return QMatIOPrivate::create(Mat_VarGetStructFieldByIndex(m_var->d, field_index, index));
}

QMatVar QMatStruct::value(QString fieldName, size_t index) const
{
	return QMatIOPrivate::create(Mat_VarGetStructFieldByName(m_var->d, qPrintable(fieldName), index));
}

size_t QMatStruct::size() const
{
	if (!m_var->d)
		return 0;
	if (m_var->d->rank < 2)
		return 1;
	return m_var->d->dims[1];
}

QVector<size_t> QMatStruct::dims() const
{
	QVector<size_t> ret(m_var->d->rank);
	for (int i = 0; i < m_var->d->rank; i++)
		ret[i] = m_var->d->dims[i];
	return ret;
}

size_t QMatStruct::dims(size_t i) const
{
	assert(i < static_cast<size_t>(m_var->d->rank));
	return m_var->d->dims[i];
}

QMatStruct::operator QMatVar() const
{
	QMatVar var;
	var.m_var = new QMatData(m_var->d);
	return var;
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<< (QDebug d, const QMatVar &v) {
	d.nospace() << "QMatVar(" << v.name() << ',';
	d.noquote() << v.dimString() << ','
					<< v.typeName() << ')';
	return d.space().quote();
}
#endif

template<typename T>
static inline QVector<T> toVector_t(matvar_t *var)
{
	size_t n1 = var->dims[0];
	size_t n2 = (var->rank > 1)?var->dims[1]:1;
	if ((n1 < 1) || (n2 < 1) || (var->nbytes == 0) || (var->class_type != Helper::matClass<T>()))
		return QVector<T>();
	QVector<T> ret(static_cast<int>(std::max(n1, n2)));
	const T *data = reinterpret_cast<const T *>(var->data);
	std::copy(data, data + ret.size(), ret.begin());
	return ret;
}

template<> QVector<double> QMatVar::toVector() const { return toVector_t<double>(m_var->d); }
template<> QVector<float> QMatVar::toVector() const { return toVector_t<float>(m_var->d); }
template<> QVector<int> QMatVar::toVector() const { return toVector_t<int>(m_var->d); }
template QMatMatrix<double> QMatVar::toMatrix(size_t) const;
template QMatMatrix<float> QMatVar::toMatrix(size_t) const;
template QMatMatrix<int> QMatVar::toMatrix(size_t) const;
