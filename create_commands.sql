CREATE TABLE FileIndex (id INTEGER PRIMARY KEY, blobid INTEGER, name TEXT, size INTEGER);
CREATE TABLE Blobs (content BLOB);
CREATE TABLE Tags (id INTEGER PRIMARY KEY, name TEXT);
