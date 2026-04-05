CREATE TABLE IF NOT EXISTS packages (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    name         TEXT NOT NULL,
    version      TEXT NOT NULL,
    type         TEXT NOT NULL CHECK(type IN ('managed', 'adopted')),
    state        TEXT NOT NULL CHECK(state IN ('active', 'inactive', 'partial')),
    installed_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    store_path   TEXT,
    UNIQUE(name, version)
);

CREATE TABLE IF NOT EXISTS files (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    package_id INTEGER NOT NULL REFERENCES packages(id),
    path       TEXT NOT NULL UNIQUE,
    is_symlink INTEGER NOT NULL CHECK(is_symlink IN (0, 1))
);

CREATE TABLE IF NOT EXISTS dependencies (
    package_id INTEGER NOT NULL REFERENCES packages(id),
    depends_on INTEGER NOT NULL REFERENCES packages(id),
    PRIMARY KEY (package_id, depends_on)
);

CREATE TABLE IF NOT EXISTS log (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp  DATETIME DEFAULT CURRENT_TIMESTAMP,
    operation  TEXT NOT NULL,
    package_id INTEGER REFERENCES packages(id),
    detail     TEXT
);

CREATE TABLE IF NOT EXISTS pending_links (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    package_id  INTEGER NOT NULL REFERENCES packages(id),
    store_path  TEXT NOT NULL,
    live_path   TEXT NOT NULL,
    state       TEXT NOT NULL CHECK(state IN ('pending', 'done'))
);
