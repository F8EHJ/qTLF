#pragma once
#include <QString>

namespace AppPaths {
// Folder containing one sub-folder per contest definition (rules/SKM, ...).
QString rulesRoot();
// SQLite database file (global database, one table of contests + one of QSOs).
QString databasePath();
}
