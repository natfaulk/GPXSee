#include <cstring>
#include <QTimeZone>
#include "common/util.h"
#include "nmeaparser.h"


static qint64 ReadLine(QFile *file, char* line, qint64 maxlen);

static bool validSentence(const char  *line, int len)
{
	const char *lp;

	if (len < 12 || line[0] != '$')
		return false;

	for (lp = line + len - 1; lp > line + 3; lp--)
		if (!::isspace(*lp))
			break;
	if (*(lp-2) != '*' || !::isalnum(*(lp-1)) || !::isalnum(*(lp)))
		return false;

	return true;
}

static bool readFloat(const char *data, int len, qreal &f)
{
	bool ok;

	f = QByteArray::fromRawData(data, len).toFloat(&ok);

	return ok;
}

bool NMEAParser::readAltitude(const char *data, int len, qreal &ele)
{
	if (!len) {
		ele = NAN;
		return true;
	}

	if (!readFloat(data, len, ele)) {
		_errorString = "Invalid altitude";
		return false;
	}

	return true;
}

bool NMEAParser::readGeoidHeight(const char *data, int len, qreal &gh)
{
	if (!len) {
		gh = 0;
		return true;
	}

	if (!readFloat(data, len, gh)) {
		_errorString = "Invalid geoid height";
		return false;
	}

	return true;
}

bool NMEAParser::readTime(const char *data, int len, QTime &time)
{
	int h, m, s, ms = 0;


	if (!len) {
		time = QTime();
		return true;
	}

	if (len < 6)
		goto error;

	h = Util::str2int(data, 2);
	m = Util::str2int(data + 2, 2);
	s = Util::str2int(data + 4, 2);
	if (h < 0 || m < 0 || s < 0)
		goto error;

	if (len > 7 && data[6] == '.') {
		if ((ms = Util::str2int(data + 7, len - 7)) < 0)
			goto error;
	}

	time = QTime(h, m, s, ms);
	if (!time.isValid())
		goto error;

	return true;

error:
	_errorString = "Invalid time";
	return false;
}

bool NMEAParser::readDate(const char *data, int len, QDate &date)
{
	int y, m, d;


	if (!len) {
		date = QDate();
		return true;
	}

	if (len < 6)
		goto error;

	d = Util::str2int(data, 2);
	m = Util::str2int(data + 2, 2);
	y = Util::str2int(data + 4, len - 4);
	if (d < 0 || m < 0 || y < 0)
		goto error;

	if (len - 4 == 2)
		date = QDate(y + 2000 < QDate::currentDate().year()
		  ? 2000 + y : 1900 + y, m, d);
	else
		date = QDate(y, m, d);
	if (!date.isValid())
		goto error;

	return true;

error:
	_errorString = "Invalid date";
	return false;
}

bool NMEAParser::readLat(const char *data, int len, qreal &lat)
{
	int d, mi;
	qreal mf;
	bool ok;


	if (!len) {
		lat = NAN;
		return true;
	}

	if (len < 7 || data[4] != '.')
		goto error;

	d = Util::str2int(data, 2);
	mi = Util::str2int(data + 2, 2);
	mf = QByteArray::fromRawData(data + 4, len - 4).toFloat(&ok);
	if (d < 0 || mi < 0 || !ok)
		goto error;

	lat = d + (((qreal)mi + mf) / 60.0);
	if (lat > 90)
		goto error;

	return true;

error:
	_errorString = "Invalid ltitude";
	return false;
}

bool NMEAParser::readNS(const char *data, int len, qreal &lat)
{
	if (!len) {
		lat = NAN;
		return true;
	}

	if (len != 1 || !(*data == 'N' || *data == 'S')) {
		_errorString = "Invalid N/S value";
		return false;
	}

	if (*data == 'S')
		lat = -lat;

	return true;
}

bool NMEAParser::readLon(const char *data, int len, qreal &lon)
{
	int d, mi;
	qreal mf;
	bool ok;


	if (!len) {
		lon = NAN;
		return true;
	}

	if (len < 8 || data[5] != '.')
		goto error;

	d = Util::str2int(data, 3);
	mi = Util::str2int(data + 3, 2);
	mf = QByteArray::fromRawData(data + 5, len - 5).toFloat(&ok);
	if (d < 0 || mi < 0 || !ok)
		goto error;

	lon = d + (((qreal)mi + mf) / 60.0);
	if (lon > 180)
		goto error;

	return true;

error:
	_errorString = "Invalid longitude";
	return false;
}

bool NMEAParser::readEW(const char *data, int len, qreal &lon)
{
	if (!len) {
		lon = NAN;
		return true;
	}

	if (len != 1 || !(*data == 'E' || *data == 'W')) {
		_errorString = "Invalid E/W value";
		return false;
	}

	if (*data == 'W')
		lon = -lon;

	return true;
}

bool NMEAParser::readRMC(CTX &ctx, const char *line, int len,
  SegmentData &segment)
{
	int col = 1;
	const char *vp = line;
	qreal lat, lon;
	QTime time;
	QDate date;
	bool valid = true;

	for (const char *lp = line; lp < line + len; lp++) {
		if (*lp == ',' || *lp == '*') {
			switch (col) {
				case 1:
					if (!readTime(vp, lp - vp, time))
						return false;
					break;
				case 2:
					if (*vp != 'A')
						valid = false;
					break;
				case 3:
					if (!readLat(vp, lp - vp, lat))
						return false;
					break;
				case 4:
					if (!readNS(vp, lp - vp, lat))
						return false;
					break;
				case 5:
					if (!readLon(vp, lp - vp, lon))
						return false;
					break;
				case 6:
					if (!readEW(vp, lp - vp, lon))
						return false;
					break;
				case 9:
					if (!readDate(vp, lp - vp, date))
						return false;
					break;
			}

			col++;
			vp = lp + 1;
		}
	}

	if (col < 9) {
		_errorString = "Invalid RMC sentence";
		return false;
	}

	if (!date.isNull()) {
		if (ctx.date.isNull() && !ctx.time.isNull() && !segment.isEmpty())
			segment.last().setTimestamp(QDateTime(date, ctx.time,
			  QTimeZone::utc()));
		ctx.date = date;
	}

	Coordinates c(lon, lat);
	if (valid && !ctx.GGA && c.isValid()) {
		Trackpoint t(c);
		if (!ctx.date.isNull() && !time.isNull())
			t.setTimestamp(QDateTime(ctx.date, time, QTimeZone::utc()));
		segment.append(t);
	}

	return true;
}

bool NMEAParser::readGGA(CTX &ctx, const char *line, int len,
  SegmentData &segment)
{
	int col = 1;
	const char *vp = line;
	qreal lat, lon, ele, gh;

	for (const char *lp = line; lp < line + len; lp++) {
		if (*lp == ',' || *lp == '*') {
			switch (col) {
				case 1:
					if (!readTime(vp, lp - vp, ctx.time))
						return false;
					break;
				case 2:
					if (!readLat(vp, lp - vp, lat))
						return false;
					break;
				case 3:
					if (!readNS(vp, lp - vp, lat))
						return false;
					break;
				case 4:
					if (!readLon(vp, lp - vp, lon))
						return false;
					break;
				case 5:
					if (!readEW(vp, lp - vp, lon))
						return false;
					break;
				case 9:
					if (!readAltitude(vp, lp - vp, ele))
						return false;
					break;
				case 10:
					if ((lp - vp) && !((lp - vp) == 1 && *vp == 'M')) {
						_errorString = "Invalid altitude units";
						return false;
					}
					break;
				case 11:
					if (!readGeoidHeight(vp, lp - vp, gh))
						return false;
					break;
				case 12:
					if ((lp - vp) && !((lp - vp) == 1 && *vp == 'M')) {
						_errorString = "Invalid geoid height units";
						return false;
					}
					break;
			}

			col++;
			vp = lp + 1;
		}
	}

	if (col < 12) {
		_errorString = "Invalid GGA sentence";
		return false;
	}

	Coordinates c(lon, lat);
	if (c.isValid()) {
		Trackpoint t(c);
		if (!(ctx.time.isNull() || ctx.date.isNull()))
			t.setTimestamp(QDateTime(ctx.date, ctx.time, QTimeZone::utc()));
		if (!std::isnan(ele))
			t.setElevation(ele - gh);
		segment.append(t);

		ctx.GGA = true;
	}

	return true;
}

bool NMEAParser::readWPL(const char *line, int len, QVector<Waypoint> &waypoints)
{
	int col = 1;
	const char *vp = line;
	qreal lat, lon;
	QString name;

	for (const char *lp = line; lp < line + len; lp++) {
		if (*lp == ',' || *lp == '*') {
			switch (col) {
				case 1:
					if (!readLat(vp, lp - vp, lat))
						return false;
					break;
				case 2:
					if (!readNS(vp, lp - vp, lat))
						return false;
					break;
				case 3:
					if (!readLon(vp, lp - vp, lon))
						return false;
					break;
				case 4:
					if (!readEW(vp, lp - vp, lon))
						return false;
					break;
				case 5:
					name = QString(QByteArray(vp, lp - vp));
					break;
			}

			col++;
			vp = lp + 1;
		}
	}

	if (col < 4) {
		_errorString = "Invalid WPL sentence";
		return false;
	}

	Coordinates c(lon, lat);
	if (c.isValid()) {
		Waypoint w(c);
		w.setName(name);
		waypoints.append(w);
	}

	return true;
}

bool NMEAParser::readZDA(CTX &ctx, const char *line, int len)
{
	int col = 1;
	const char *vp = line;
	int d = 0, m = 0, y = 0;

	for (const char *lp = line; lp < line + len; lp++) {
		if (*lp == ',' || *lp == '*') {
			switch (col) {
				case 2:
					if (!(lp - vp))
						return true;
					if ((d = Util::str2int(vp, lp - vp)) < 0) {
						_errorString = "Invalid day";
						return false;
					}
					break;
				case 3:
					if (!(lp - vp))
						return true;
					if ((m = Util::str2int(vp, lp - vp)) < 0) {
						_errorString = "Invalid month";
						return false;
					}
					break;
				case 4:
					if (!(lp - vp))
						return true;
					if ((y = Util::str2int(vp, lp - vp)) < 0) {
						_errorString = "Invalid year";
						return false;
					}
					break;
			}

			col++;
			vp = lp + 1;
		}
	}

	if (col < 4) {
		_errorString = "Invalid ZDA sentence";
		return false;
	}

	ctx.date = QDate(y, m, d);
	if (!ctx.date.isValid()) {
		_errorString = "Invalid date";
		return false;
	}

	return true;
}

bool NMEAParser::parse(QFile *file, QList<TrackData> &tracks,
  QList<RouteData> &routes, QList<Area> &polygons,
  QVector<Waypoint> &waypoints)
{
	Q_UNUSED(routes);
	Q_UNUSED(polygons);
	qint64 len;
	char line[80 + 2/*CRLF*/ + 1/*'\0'*/ + 1/*extra byte for limit check*/];
	SegmentData segment;
	CTX ctx;
	

	_errorLine = 1;
	_errorString.clear();

	while (!file->atEnd()) {
		len = ReadLine(file, line, sizeof(line));

		if (len < 0) {
			_errorString = "I/O error";
			return false;
		} else if (len >= (qint64)sizeof(line) - 1)
		 	continue;

		if (validSentence(line, len)) {
			if (!memcmp(line + 3, "RMC,", 4)) {
				if (!readRMC(ctx, line + 7, len - 7, segment))
					continue;
			} else if (!memcmp(line + 3, "GGA,", 4)) {
				if (!readGGA(ctx, line + 7, len - 7, segment))
					continue;
			} else if (!memcmp(line + 3, "WPL,", 4)) {
				if (!readWPL(line + 7, len - 7, waypoints))
					continue;
			} else if (!memcmp(line + 3, "ZDA,", 4)) {
				if (!readZDA(ctx, line + 7, len - 7))
					continue;
			}
		}

		_errorLine++;
	}

	if (!segment.size() && !waypoints.size()) {
		_errorString = "No usable NMEA sentence found";
		return false;
	}

	if (segment.size()) {
		tracks.append(TrackData());
		tracks.last().setFile(file->fileName());
		tracks.last().append(segment);
	}

	return true;
}

static qint64 ReadLine(QFile *file, char* line, qint64 maxlen)
{
	qint64 len = 0;
	char c;

	while (len < (maxlen - 1) && file->getChar(&c)) {
		if (c == '\n' || c =='\r'){
			line[len] = '\0';
			return len;
		} else {
			line[len] = c;
			++len;
		}
	}

	line[len] = '\0';
	return len;
}
