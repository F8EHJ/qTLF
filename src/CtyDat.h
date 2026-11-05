#pragma once

#include <QHash>
#include <QString>
#include <QVariantMap>
#include <QVector>

// Country file reader (cty.dat format of country-files.com, AD1C):
//   https://www.country-files.com/cty-dat-format/
// Gives the DXCC entity, continent and zones of a callsign.
class CtyDat
{
public:
    struct Entity {
        QString name;
        QString continent;      // EU AF AS NA SA OC AN
        int cqZone = 0;
        int ituZone = 0;
        QString prefix;         // primary DXCC prefix ('*' WAE flag removed)
        bool wae = false;
    };
    struct Info {
        bool valid = false;
        QString country, continent, prefix;
        int cqZone = 0, ituZone = 0;
        QVariantMap toVariant() const;
    };

    bool load(const QString &path, QString *error = nullptr);
    bool isLoaded() const { return !m_entities.isEmpty(); }
    int entityCount() const { return m_entities.size(); }

    Info lookup(const QString &callsign) const;

private:
    struct Alias {
        int entity = -1;
        QString continent;      // overrides (empty = entity value)
        int cqZone = 0, ituZone = 0;
    };
    Info make(const Alias &a) const;
    bool findPrefix(const QString &s, Alias &out) const;

    QVector<Entity> m_entities;
    QHash<QString, Alias> m_exact;      // "=CALL" entries
    QHash<QString, Alias> m_prefixes;
    int m_maxPrefix = 0;
};
