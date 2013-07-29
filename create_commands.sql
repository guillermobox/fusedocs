CREATE TABLE FileIndex (id INTEGER PRIMARY KEY, blobid INTEGER, name TEXT);
CREATE TABLE Blobs (content BLOB);
INSERT INTO FileIndex (blobid, name) VALUES (1, "test.txt");
INSERT INTO Blobs VALUES ("Example of content");
