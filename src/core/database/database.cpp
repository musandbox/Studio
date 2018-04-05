#include "database.h"
#include "../../constants.h"
#include "../../irisgl/src/irisglfwd.h"
#include "../../irisgl/src/core/irisutils.h"
#include "../../globals.h"
#include "../guidmanager.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlRecord>
#include <QDateTime>
#include <QMessageBox>

Database::Database()
{
    if (!QSqlDatabase::isDriverAvailable(Constants::DB_DRIVER)) irisLog("DB driver not present!");

    db = QSqlDatabase::addDatabase(Constants::DB_DRIVER);
}

Database::~Database()
{
    auto connection = db.connectionName();
    db.close();
    db = QSqlDatabase();
    db.removeDatabase(connection);
}

bool Database::executeAndCheckQuery(QSqlQuery &query, const QString& name)
{
    if (!query.exec()) {
        irisLog(name + " + Query failed to execute: " + query.lastError().text());
        return false;
    }

    return true;
}

void Database::initializeDatabase(QString name)
{
    db.setDatabaseName(name);
    if (!db.open()) {
        irisLog( "Couldn't open a DB connection. " + db.lastError().text());
    }
}

void Database::closeDb()
{
    auto connection = db.connectionName();
    db.close();
    db = QSqlDatabase();
    db.removeDatabase(connection);
}

bool Database::checkIfTableExists(const QString &tableName)
{
    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = ?;");
    query.addBindValue(tableName);

    if (query.exec()) {
        if (query.first()) {
            return query.value(0).toBool();
        }
    }
    else {
        irisLog("There was an error getting the material blob! " + query.lastError().text());
    }

    return false;
}

void Database::createGlobalDependencies()
{
	QString schema = "CREATE TABLE IF NOT EXISTS dependencies ("
		"	 depender_type  INTEGER,"
		"	 dependee_type  INTEGER,"
		"    project_guid	VARCHAR(32),"
		"    depender		VARCHAR(32),"
		"    dependee		VARCHAR(32),"
		"    id				VARCHAR(32) PRIMARY KEY"
		")";

	QSqlQuery query;
	query.prepare(schema);
	executeAndCheckQuery(query, "createGlobalDbDependencies");
}

void Database::insertGlobalDependency(const int ertype, const int eetype, const QString &depender, const QString &dependee, const QString &project_guid)
{
	QSqlQuery query;
	auto guid = GUIDManager::generateGUID();
	query.prepare("INSERT INTO dependencies (depender_type, dependee_type, project_guid, depender, dependee, id) VALUES (:depender_type, :dependee_type, :project_guid, :depender, :dependee, :id)");

	query.bindValue(":depender_type", ertype);
	query.bindValue(":dependee_type", eetype);
	if (!project_guid.isEmpty()) query.bindValue(":project_guid", project_guid);
	query.bindValue(":depender", depender);
	query.bindValue(":dependee", dependee);
	query.bindValue(":id", guid);

	executeAndCheckQuery(query, "insertGlobalDependency");
}

void Database::updateGlobalDependencyDepender(const int &ertype, const QString & depender, const QString & dependee)
{
    QSqlQuery query;
    auto guid = GUIDManager::generateGUID();
    query.prepare("UPDATE dependencies SET depender = ? WHERE depender_type = ? AND dependee = ?");

    query.bindValue(":depender", depender);
    query.bindValue(":type", ertype);
    query.bindValue(":dependee", dependee);

    executeAndCheckQuery(query, "updateGlobalDependencyDepender");
}

void Database::updateGlobalDependencyDependee(const int & ertype, const QString & depender, const QString & dependee)
{
    QSqlQuery query;
    auto guid = GUIDManager::generateGUID();
    query.prepare("UPDATE dependencies SET dependee = ? WHERE depender_type = ? AND depender = ?");

    query.bindValue(":depender", depender);
    query.bindValue(":type", ertype);
    query.bindValue(":dependee", dependee);

    executeAndCheckQuery(query, "updateGlobalDependencyDependee");
}

QString Database::getDependencyByType(const int &ertype, const QString &depender)
{
	QSqlQuery query;
	query.prepare("SELECT dependee FROM dependencies WHERE depender_type = ? AND depender = ?");
	query.addBindValue(ertype);
	query.addBindValue(depender);

	if (query.exec()) {
		if (query.first()) {
			return query.value(0).toString();
		}
	}
	else {
		irisLog("There was an error getting the dependee id! " + query.lastError().text());
	}

	return QString();
}

void Database::createGlobalDb() {
    QString schema = "CREATE TABLE IF NOT EXISTS " + Constants::DB_PROJECTS_TABLE + " ("
                     "    name              VARCHAR(64),"
                     "    thumbnail         BLOB,"
                     "    last_accessed     DATETIME,"
                     "    last_written      DATETIME,"
                     "    date_created      DATETIME DEFAULT CURRENT_TIMESTAMP,"
                     "    scene             BLOB,"
					 "    version           VARCHAR(8),"
                     "    description       TEXT,"
                     "    url               TEXT,"
                     "    guid              VARCHAR(32) PRIMARY KEY"
                     ")";

    QSqlQuery query;
    query.prepare(schema);
    executeAndCheckQuery(query, "createGlobalDb");
}

void Database::createGlobalDbThumbs() {
    QString schema = "CREATE TABLE IF NOT EXISTS " + Constants::DB_THUMBS_TABLE + " ("
                     "    name              VARCHAR(128),"
                     "    world_guid        VARCHAR(32),"
                     "    thumbnail         BLOB,"
                     "    last_written      DATETIME,"
                     "    hash              VARCHAR(16),"
                     "    guid              VARCHAR(32) PRIMARY KEY"
                     ")";

    QSqlQuery query;
    query.prepare(schema);
    executeAndCheckQuery(query, "createGlobalDbThumbs");
}

void Database::createGlobalDbCollections()
{
    if (!checkIfTableExists(Constants::DB_COLLECT_TABLE)) {
        QString schema = "CREATE TABLE IF NOT EXISTS " + Constants::DB_COLLECT_TABLE + " ("
            "    name              VARCHAR(128),"
            "    date_created      DATETIME DEFAULT CURRENT_TIMESTAMP,"
            "    collection_id     INTEGER PRIMARY KEY"
            ")";

        QSqlQuery query;
        query.prepare(schema);
        executeAndCheckQuery(query, "createGlobalDbCollections");

        QSqlQuery query2;
        query.prepare("INSERT INTO " + Constants::DB_COLLECT_TABLE +
            " (name, date_created, collection_id)" +
            " VALUES (:name, datetime(), 0)");
        query.bindValue(":name", "Uncategorized");

        executeAndCheckQuery(query, "insertSceneCollection");
    }
}

/*
 *	properties is a json object that currently holds
 *  1. the camera orientation
 *  2. number of textures
 *	3. polygon count
 */
void Database::createGlobalDbAssets() {
	QString schema = "CREATE TABLE IF NOT EXISTS assets ("
		"    guid              VARCHAR(32) PRIMARY KEY,"
		"	 type			   INTEGER,"
		"    name              VARCHAR(128),"
		"	 collection		   INTEGER,"
		"	 times_used		   INTEGER,"
		"    project_guid      VARCHAR(32),"
		"    date_created      DATETIME DEFAULT CURRENT_TIMESTAMP,"
		"    last_updated      DATETIME,"
		"	 author			   VARCHAR(128),"
		"    license		   VARCHAR(64),"
		"    hash              VARCHAR(16),"
		"    version           VARCHAR(8),"
		"    parent            VARCHAR(32),"
		"    tags			   BLOB,"
		"    properties        BLOB,"
		"    asset             BLOB,"
		"    thumbnail         BLOB"
		")";

	QSqlQuery query;
	query.prepare(schema);
	executeAndCheckQuery(query, "createGlobalDbAssets");
}

QString Database::insertAssetGlobal(
	const QString &assetName,
	int type,
	const QString &parentFolder,
	const QByteArray &thumbnail,
	const QByteArray &properties,
	const QByteArray &tags,
	const QByteArray &asset,
	const QString &author)
{
	auto guid = GUIDManager::generateGUID();

	QSqlQuery query;
	query.prepare(
		"INSERT INTO assets"
		" (name, thumbnail, parent, type, collection, version, date_created,"
		" last_updated, guid, properties, author, asset, license, tags)"
		" VALUES (:name, :thumbnail, :parent, :type, 0, :version, datetime(),"
		" datetime(), :guid, :properties, :author, :asset, :license, :tags)"
	);

	query.bindValue(":name", assetName);
	query.bindValue(":thumbnail", thumbnail);
	query.bindValue(":parent", parentFolder);
	query.bindValue(":type", type);
	query.bindValue(":version", Constants::CONTENT_VERSION);
	query.bindValue(":guid", guid);
	query.bindValue(":properties", properties);
	query.bindValue(":author", author);// getAuthorName());
	query.bindValue(":asset", asset);
	query.bindValue(":license", "CCBY");
	query.bindValue(":tags", tags);

	executeAndCheckQuery(query, "insertSceneAsset");

	return guid;
}

QString Database::createAssetEntry(
	const QString &guid,
	const QString &assetName,
	int type,
	const QString &project_guid,
	const QString &parentFolder,
	const QByteArray &thumbnail,
	const QByteArray &properties,
	const QByteArray &tags,
	const QByteArray &asset,
	const QString &author)
{
	QSqlQuery query;
	query.prepare(
		"INSERT INTO assets"
		" (name, thumbnail, parent, type, project_guid, collection, version, date_created,"
		" last_updated, guid, properties, author, asset, license, tags)"
		" VALUES (:name, :thumbnail, :parent, :type, :project_guid, 0, :version, datetime(),"
		" datetime(), :guid, :properties, :author, :asset, :license, :tags)"
	);

	query.bindValue(":name",		assetName);
	query.bindValue(":thumbnail",	thumbnail);
	query.bindValue(":parent",		parentFolder);
	query.bindValue(":type",		type);
	query.bindValue(":project_guid",project_guid);
	query.bindValue(":version",		Constants::CONTENT_VERSION);
	query.bindValue(":guid",		guid);
	query.bindValue(":properties",	properties);
	query.bindValue(":author",		author);// getAuthorName());
	query.bindValue(":asset",		asset);
	query.bindValue(":license",		"CCBY");
	query.bindValue(":tags",		tags);

	executeAndCheckQuery(query, "createAssetEntry");

	return guid;
}

// assets.name = file.obj (remove ext)
// assets.extension = file.obj (suffix)
QVector<AssetTileData> Database::fetchAssets()
{
	QSqlQuery query;
	query.prepare(
		"SELECT assets.name, assets.thumbnail, assets.guid, collections.name as collection_name, "
		"assets.type, assets.collection, assets.properties, assets.author, assets.license, "
        "assets.tags, assets.project_guid "
        "FROM assets "
		"INNER JOIN collections ON assets.collection = collections.collection_id "
        "WHERE assets.type = :m OR assets.type = :o "
		"ORDER BY assets.name DESC"
	);
    query.bindValue(":m", static_cast<int>(ModelTypes::Object));
    query.bindValue(":o", static_cast<int>(ModelTypes::Material));
	executeAndCheckQuery(query, "fetchAssets");

	QVector<AssetTileData> tileData;
	while (query.next()) {
		AssetTileData data;
		QSqlRecord record = query.record();
		for (int i = 0; i < record.count(); i++) {
			data.name = record.value(0).toString();
			data.thumbnail = record.value(1).toByteArray();
			data.guid = record.value(2).toString();
			data.collection_name = record.value(3).toString();
			data.type = record.value(4).toInt();
			data.collection = record.value(5).toInt();
			data.properties = record.value(6).toByteArray();
			data.author = record.value(7).toString();
			data.license = record.value(8).toString();
			data.tags = record.value(9).toByteArray();
		}

		Globals::assetNames.insert(data.guid, data.name);

		tileData.push_back(data);
	}

	return tileData;
}

AssetTileData Database::fetchAsset(const QString &guid)
{
	QSqlQuery query;
	query.prepare(
		"SELECT name, thumbnail, guid, parent, type FROM assets WHERE guid = ? "
	);
	query.addBindValue(guid);
	executeAndCheckQuery(query, "fetchAsset");

	if (query.exec()) {
		if (query.first()) {
			AssetTileData data;
			data.name = query.value(0).toString();
			data.thumbnail = query.value(1).toByteArray();
			data.guid = query.value(2).toString();
			data.parent = query.value(3).toString();
			data.type = query.value(4).toInt();
			return data;
		}
	}
	else {
		irisLog("There was an error getting the asset! " + query.lastError().text());
	}

	return AssetTileData();
}

QVector<FolderData> Database::fetchCrumbTrail(const QString &guid)
{
	std::function<void(QVector<FolderData>&, const QString&)> fetchFolders
		= [&](QVector<FolderData> &folders, const QString &guid) -> void
	{
		QSqlQuery query;
		query.prepare("SELECT guid, parent, name FROM folders WHERE guid = ?");
		query.addBindValue(guid);
		executeAndCheckQuery(query, "fetchCrumbTrail");

		QStringList parentFolder;
		while (query.next()) {
			FolderData data;
			QSqlRecord record = query.record();
			data.guid = record.value(0).toString();
			data.parent = record.value(1).toString();
			data.name = record.value(2).toString();

			parentFolder.push_back(data.parent);
			folders.push_back(data);
		}

		for (const QString &folder : parentFolder) {
			fetchFolders(folders, folder);
		}
	};

	QVector<FolderData> folders;
	fetchFolders(folders, guid);

	FolderData home;
	home.guid = Globals::project->getProjectGuid();
	home.name = "Assets";
	folders.push_back(home);

	std::reverse(folders.begin(), folders.end());

	return folders;
}

QVector<FolderData> Database::fetchChildFolders(const QString &parent)
{
	QSqlQuery query;
	query.prepare("SELECT guid, parent, name, count FROM folders WHERE parent = ?");
	query.addBindValue(parent);
	executeAndCheckQuery(query, "fetchChildFolders");

	QVector<FolderData> folderData;
	while (query.next()) {
		FolderData data;
		QSqlRecord record = query.record();
		for (int i = 0; i < record.count(); i++) {
			data.guid = record.value(0).toString();
			data.parent = record.value(1).toString();
			data.name = record.value(2).toString();
			data.count = record.value(3).toInt();
		}

		folderData.push_back(data);
	}

	return folderData;
}

QVector<AssetTileData> Database::fetchChildAssets(const QString &parent, bool showDependencies)
{
	QSqlQuery query;
	if (showDependencies) {
		query.prepare(
			"SELECT name, thumbnail, guid, parent, type "
			"FROM assets A WHERE parent = ? "
			"ORDER BY A.name DESC"
		);
	}
	else {
		query.prepare(
			"SELECT name, thumbnail, guid, parent, type "
			"FROM assets A WHERE parent = ? "
			"AND A.guid NOT IN (SELECT dependee FROM dependencies) "
			"ORDER BY A.name DESC"
		);
	}
	query.addBindValue(parent);
	executeAndCheckQuery(query, "fetchChildAssets");

	QVector<AssetTileData> tileData;
	while (query.next()) {
		AssetTileData data;
		QSqlRecord record = query.record();
		for (int i = 0; i < record.count(); i++) {
			data.name = record.value(0).toString();
			data.thumbnail = record.value(1).toByteArray();
			data.guid = record.value(2).toString();
			data.parent = record.value(3).toString();
			data.type = record.value(4).toInt();
		}

		tileData.push_back(data);
	}

	return tileData;
}

QVector<AssetTileData> Database::fetchAssetsByCollection(int collection_id)
{
	QSqlQuery query;
	query.prepare(
		"SELECT assets.name,"
		" assets.thumbnail, assets.guid, collections.name as collection_name, assets.type,"
		" assets.author, assets.license, assets.tags"
		" FROM assets"
		" INNER JOIN collections ON assets.collection = collections.collection_id  WHERE assets.type = 5"
		" ORDER BY assets.name DESC WHERE assets.collection_id = ?");
	query.addBindValue(collection_id);
	executeAndCheckQuery(query, "fetchAssetsByCollection");

	QVector<AssetTileData> tileData;
	while (query.next()) {
		AssetTileData data;
		QSqlRecord record = query.record();
		for (int i = 0; i < record.count(); i++) {
			data.name = record.value(0).toString();
			data.thumbnail = record.value(1).toByteArray();
			data.guid = record.value(2).toString();
			data.collection_name = record.value(3).toString();
			data.type = record.value(4).toInt();

			data.full_filename = data.guid + "." + QFileInfo(data.name).suffix();
		}

		Globals::assetNames.insert(data.guid, data.name);

		tileData.push_back(data);
	}

	return tileData;
}

QVector<AssetTileData> Database::fetchAssetsByType(const int type)
{
    QSqlQuery query;
    query.prepare("SELECT guid, type, name, asset FROM assets WHERE type = ?");
    query.addBindValue(type);
    executeAndCheckQuery(query, "fetchAssetsByType");

    QVector<AssetTileData> tileData;
    while (query.next()) {
        AssetTileData data;
        QSqlRecord record = query.record();
        for (int i = 0; i < record.count(); i++) {
            data.guid = record.value(0).toString();
            data.type = record.value(1).toInt();
            data.name = record.value(2).toString();
            data.asset = record.value(3).toByteArray();
        }

        tileData.push_back(data);
    }

    return tileData;
}

QVector<AssetData> Database::fetchAssetThumbnails(const QStringList &guids)
{
	// Construct the guid list to use and chop of the extraneous comma to make it valid
	QString guidInString;
	for (const QString &guid : guids) guidInString += "'" + guid + "',";
	guidInString.chop(1);

	QSqlQuery query;
	query.prepare("SELECT guid, thumbnail, name FROM assets WHERE guid IN (" + guidInString + ")");
	executeAndCheckQuery(query, "fetchAssetThumbnails");

	QVector<AssetData> assetData;
	while (query.next()) {
		AssetData data;
		QSqlRecord record = query.record();
		for (int i = 0; i < record.count(); i++) {
			data.guid		= record.value(0).toString();
			data.thumbnail	= record.value(1).toByteArray();
			data.name		= record.value(2).toString();
		}

		assetData.push_back(data);
	}

	return assetData;
}

void Database::createGlobalDbAuthor()
{
	QString schema = "CREATE TABLE IF NOT EXISTS author ("
		"    name              VARCHAR(128),"
		"    default_license   VARCHAR(24),"
		"    date_created      DATETIME DEFAULT CURRENT_TIMESTAMP,"
		"    last_updated      DATETIME,"
		"    version           VARCHAR(8)"
		")";

	QSqlQuery query;
	query.prepare(schema);
	executeAndCheckQuery(query, "createGlobalDbAuthor");
}

void Database::createGlobalDbFolders()
{
	QString schema = "CREATE TABLE IF NOT EXISTS folders ("
		"    guid              VARCHAR(32) PRIMARY KEY,"
		"    parent            VARCHAR(32),"
		"    name              VARCHAR(128),"
		"    date_created      DATETIME DEFAULT CURRENT_TIMESTAMP,"
		"    last_updated      DATETIME,"
		"    hash              VARCHAR(16),"
		"    version           VARCHAR(8),"
		"    count			   INTEGER,"
		"    visible           INTEGER"
		")";

	QSqlQuery query;
	query.prepare(schema);
	executeAndCheckQuery(query, "createGlobalDbFolder");
}

void Database::updateAuthorInfo(const QString &author_name)
{
	QSqlQuery query1;
	query1.prepare("DELETE FROM author");
	executeAndCheckQuery(query1, "wipeTable");

	QSqlQuery query2;
	query2.prepare("INSERT INTO author (name, date_created, default_license) VALUES (:name, datetime(), :default_license)");
	query2.bindValue(":name", author_name);
	query2.bindValue(":default_license", "CCBY");
	executeAndCheckQuery(query2, "insertAuthorName");
}

bool Database::isAuthorInfoPresent()
{
	QSqlQuery query;
	query.prepare("SELECT COUNT(*) FROM author");
	executeAndCheckQuery(query, "authorCount");

	if (query.exec()) {
		if (query.first()) {
			return query.value(0).toBool();
		}
	}
	else {
		irisLog("There was an error getting the author count! " + query.lastError().text());
	}

	return false;
}

QString Database::getAuthorName()
{
	QSqlQuery query;
	query.prepare("SELECT name FROM author LIMIT 1");
	executeAndCheckQuery(query, "getAuthorName");

	if (query.exec()) {
		if (query.first()) {
			return query.value(0).toString();
		}
	}
	else {
		irisLog("There was an error getting the author count! " + query.lastError().text());
	}

	return QString();
}

QString Database::insertMaterialGlobal(const QString &materialName, const QString &asset_guid, const QByteArray &material)
{
	QSqlQuery query;
	auto guid = GUIDManager::generateGUID();
	query.prepare("INSERT INTO assets (name, date_created, type, collection, version, asset, guid)"
				  " VALUES (:name, datetime(), :type, 0, :version, :asset, :guid)");
	query.bindValue(":name", materialName);
	query.bindValue(":type", 1); // switch this to the enum later
	query.bindValue(":version", Constants::CONTENT_VERSION); // switch this to the enum later
	query.bindValue(":asset", material);
	query.bindValue(":guid", guid);

	executeAndCheckQuery(query, "insertMaterialGlobal");

	return guid;
}

QString Database::insertProjectMaterialGlobal(const QString & materialName, const QString & asset_guid, const QByteArray & material)
{
	QSqlQuery query;
	auto guid = GUIDManager::generateGUID();
	query.prepare(
		"INSERT INTO assets (name, date_created, type, collection, version, asset, guid, project_guid) "
		"VALUES (:name, datetime(), :type, 0, :version, :asset, :guid, :project_guid)");
	query.bindValue(":name", materialName);
	query.bindValue(":type", 1); // switch this to the enum later
	query.bindValue(":version", Constants::CONTENT_VERSION); // switch this to the enum later
	query.bindValue(":asset", material);
	query.bindValue(":guid", guid);
	query.bindValue(":project_guid", Globals::project->getProjectGuid());

	executeAndCheckQuery(query, "insertMaterialGlobal");

	return guid;
}

void Database::deleteProject()
{
    QSqlQuery query;
    query.prepare("DELETE FROM " + Constants::DB_PROJECTS_TABLE + " WHERE guid = ?");
    query.addBindValue(Globals::project->getProjectGuid());
    executeAndCheckQuery(query, "deleteProject");
}

bool Database::deleteAsset(const QString &guid)
{
    QSqlQuery query;
    query.prepare("DELETE FROM assets WHERE guid = ?");
    query.addBindValue(guid);

	return executeAndCheckQuery(query, "deleteAsset");
}

bool Database::deleteFolder(const QString &guid)
{
	QSqlQuery query;
	query.prepare("DELETE FROM folders WHERE guid = ?");
	query.addBindValue(guid);

	return executeAndCheckQuery(query, "deleteFolder");
}


void Database::renameProject(const QString &newName)
{
    QSqlQuery query;
    query.prepare("UPDATE " + Constants::DB_PROJECTS_TABLE + " SET name = ? WHERE guid = ?");
    query.addBindValue(newName);
    query.addBindValue(Globals::project->getProjectGuid());
    executeAndCheckQuery(query, "renameProject");
}

void Database::updateAssetThumbnail(const QString guid, const QByteArray &thumbnail)
{
	QSqlQuery query;
	query.prepare("UPDATE assets SET thumbnail = ? WHERE guid = ?");
	query.addBindValue(thumbnail);
	query.addBindValue(guid);
	executeAndCheckQuery(query, "updateAssetThumbnail");
}

void Database::updateAssetAsset(const QString guid, const QByteArray &asset)
{
	QSqlQuery query;
	query.prepare("UPDATE assets SET asset = ? WHERE guid = ?");
	query.addBindValue(asset);
	query.addBindValue(guid);
	executeAndCheckQuery(query, "updateAssetAsset");
}

void Database::updateAssetProperties(const QString guid, const QByteArray &asset)
{
    QSqlQuery query;
    query.prepare("UPDATE assets SET properties = ? WHERE guid = ?");
    query.addBindValue(asset);
    query.addBindValue(guid);
    executeAndCheckQuery(query, "updateAssetProperties");
}

QString Database::insertFolder(const QString &folderName, const QString &parentFolder, const QString &guid)
{
	QSqlQuery query;
	query.prepare(
		"INSERT INTO folders (name, parent, version, date_created, last_updated, guid) "
		"VALUES (:name, :parent, :version, datetime(), datetime(), :guid)"
	);

	query.bindValue(":name",	folderName);
	query.bindValue(":parent",  parentFolder);
	query.bindValue(":version", Constants::CONTENT_VERSION);
	query.bindValue(":guid",	guid);

	executeAndCheckQuery(query, "insertFolder");

	return guid;
}

void Database::insertCollectionGlobal(const QString &collectionName)
{
    QSqlQuery query;
    auto guid = GUIDManager::generateGUID();
    query.prepare("INSERT INTO " + Constants::DB_COLLECT_TABLE +
        " (name, date_created)" +
        " VALUES (:name, datetime())");
    query.bindValue(":name", collectionName);

    executeAndCheckQuery(query, "insertSceneCollection");
}

bool Database::switchAssetCollection(const int id, const QString &guid)
{
    QSqlQuery query;
    query.prepare("UPDATE " + Constants::DB_ASSETS_TABLE + " SET collection = ?, last_updated = datetime() WHERE guid = ?");
    query.addBindValue(id);
    query.addBindValue(guid);

    return executeAndCheckQuery(query, "switchAssetCollection");
}

void Database::insertProjectAssetGlobal(const QString &assetName,
										int type,
										const QByteArray &thumbnail,
										const QByteArray &properties,
										const QByteArray &tags,
										const QByteArray &asset,
								        const QString &guid)
{
	QSqlQuery query;
	query.prepare(
		"INSERT INTO assets (name, thumbnail, type, collection, version, date_created, "
		"last_updated, project_guid, guid, properties, author, asset, license, tags) "
		"VALUES (:name, :thumbnail, :type, 0, :version, datetime(), "
		"datetime(), :project_guid, :guid, :properties, :author, :asset, :license, :tags)");

	QFileInfo assetInfo(assetName);

	query.bindValue(":name", assetInfo.fileName());
	query.bindValue(":thumbnail", thumbnail);
	query.bindValue(":type", type);
	query.bindValue(":version", Constants::CONTENT_VERSION);
	query.bindValue(":project_guid", Globals::project->getProjectGuid());
	query.bindValue(":guid", guid);
	query.bindValue(":properties", properties);

	query.bindValue(":author", "");// getAuthorName());
	query.bindValue(":asset", asset);// getAuthorName());
	query.bindValue(":license", "CCBY");
	query.bindValue(":tags", tags);

	executeAndCheckQuery(query, "insertProjectSceneAsset");
}

void Database::insertSceneGlobal(const QString &projectName, const QByteArray &sceneBlob, const QByteArray &thumb)
{
    QSqlQuery query;
    auto guid = GUIDManager::generateGUID();
    query.prepare("INSERT INTO " + Constants::DB_PROJECTS_TABLE                 +
                  " (name, scene, thumbnail, version, last_accessed, last_written, guid)"   +
                  " VALUES (:name, :scene, :thumb, :version, datetime(), datetime(), :guid)");
    query.bindValue(":name",    projectName);
    query.bindValue(":scene",   sceneBlob);
	query.bindValue(":thumb",	thumb);
    query.bindValue(":version", Constants::CONTENT_VERSION);
    query.bindValue(":guid",    guid);

    executeAndCheckQuery(query, "insertSceneGlobal");

    Globals::project->setProjectGuid(guid);
}

void Database::insertThumbnailGlobal(const QString &world_guid,
                                     const QString &name,
                                     const QByteArray &thumbnail,
								     const QString &thumbnail_guid)
{
    QSqlQuery query;
    query.prepare("INSERT INTO " + Constants::DB_THUMBS_TABLE + " (world_guid, name, thumbnail, guid)"
                  " VALUES (:world_guid, :name, :thumbnail, :guid)");
    query.bindValue(":world_guid",  world_guid);
    query.bindValue(":thumbnail",   thumbnail);
    query.bindValue(":name",        name);
    query.bindValue(":guid",        thumbnail_guid);

    executeAndCheckQuery(query, "insertThumbnailGlobal");
}

QByteArray Database::getMaterialGlobal(const QString &guid) const
{
	QSqlQuery query;
	query.prepare("SELECT asset FROM assets WHERE guid = ?");
	query.addBindValue(guid);

	if (query.exec()) {
		if (query.first()) {
			return query.value(0).toByteArray();
		}
	}
	else {
		irisLog("There was an error getting the material blob! " + query.lastError().text());
	}

	return QByteArray();
}

QByteArray Database::getAssetMaterialGlobal(const QString &guid) const
{
	QSqlQuery query;
	query.prepare("SELECT asset FROM assets WHERE guid = ?");
	query.addBindValue(guid);

	if (query.exec()) {
		if (query.first()) {
			return query.value(0).toByteArray();
		}
	}
	else {
		irisLog("There was an error getting the material blob! " + query.lastError().text());
	}

	return QByteArray();
}

bool Database::hasCachedThumbnail(const QString &name)
{
    QSqlQuery query;
    query.prepare("SELECT EXISTS (SELECT 1 FROM " + Constants::DB_THUMBS_TABLE + " WHERE name = ? LIMIT 1)");
    query.addBindValue(name);

    if (query.exec()) {
        if (query.first()) {
            return query.record().value(0).toBool();
        }
    } else {
        irisLog("hasCachedThumbnail query failed! " + query.lastError().text());
    }

    return false;
}

QVector<AssetData> Database::fetchThumbnails()
{
	QSqlQuery query;
	query.prepare("SELECT name, thumbnail, guid, type FROM assets WHERE type = 5");
	executeAndCheckQuery(query, "fetchThumbnails");

	QVector<AssetData> tileData;
	while (query.next()) {
		AssetData data;
		QSqlRecord record = query.record();
		for (int i = 0; i < record.count(); i++) {
			data.name		= record.value(0).toString();
			data.thumbnail	= record.value(1).toByteArray();
			data.guid		= record.value(2).toString();
			data.type		= record.value(3).toInt();
			data.extension  = QFileInfo(data.name).suffix();
		}

		tileData.push_back(data);
	}

	return tileData;
}

QVector<AssetData> Database::fetchFilteredAssets(const QString &guid, int type)
{
	QSqlQuery query;
	query.prepare("SELECT name, guid FROM assets WHERE project_guid = ? AND type = ?");
	query.addBindValue(guid);
	query.addBindValue(type);
	executeAndCheckQuery(query, "fetchFilteredAssets");

	QVector<AssetData> tileData;
	while (query.next()) {
		AssetData data;
		QSqlRecord record = query.record();
		for (int i = 0; i < record.count(); i++) {
			data.name = record.value(0).toString();
			data.guid = record.value(1).toString();
		}

		tileData.push_back(data);
	}

	return tileData;
}

QVector<CollectionData> Database::fetchCollections()
{
    QSqlQuery query;
    query.prepare("SELECT name, collection_id FROM " + Constants::DB_COLLECT_TABLE + " ORDER BY name, date_created DESC");
    executeAndCheckQuery(query, "fetchCollections");

    QVector<CollectionData> tileData;
    while (query.next()) {
        CollectionData data;
        QSqlRecord record = query.record();
        for (int i = 0; i < record.count(); i++) {
            data.name = record.value(0).toString();
            data.id = record.value(1).toInt();
        }

        tileData.push_back(data);
    }

    return tileData;
}

QVector<ProjectTileData> Database::fetchProjects()
{
    QSqlQuery query;
    query.prepare("SELECT name, thumbnail, guid FROM projects ORDER BY last_written DESC");
    executeAndCheckQuery(query, "fetchProjects");

    QVector<ProjectTileData> tileData;
    while (query.next())  {
        ProjectTileData data;
        QSqlRecord record = query.record();
        for (int i = 0; i < record.count(); i++) {
            data.name       = record.value(0).toString();
            data.thumbnail  = record.value(1).toByteArray();
            data.guid       = record.value(2).toString();
        }

        tileData.push_back(data);
    }

    return tileData;
}

QByteArray Database::getSceneBlobGlobal() const
{
    QSqlQuery query;
    query.prepare("SELECT scene FROM " + Constants::DB_PROJECTS_TABLE + " WHERE guid = ?");
    query.addBindValue(Globals::project->getProjectGuid());

    if (query.exec()) {
        if (query.first()) {
            return query.value(0).toByteArray();
        }
    } else {
        irisLog("There was an error getting the scene blob! " + query.lastError().text());
    }

    return QByteArray();
}

QByteArray Database::fetchCachedThumbnail(const QString &name) const
{
    QSqlQuery query;
    query.prepare("SELECT thumbnail FROM " + Constants::DB_THUMBS_TABLE + " WHERE name = ?");
    query.addBindValue(name);

    if (query.exec()) {
        if (query.first()) {
            return query.value(0).toByteArray();
        }
    } else {
        irisLog(
            "There was an error fetching a thumbnail for a model (" + name + ")" + query.lastError().text()
        );
    }

    return QByteArray();
}

void Database::updateAssetMetadata(const QString &guid, const QString &name, const QByteArray &tags)
{
	QSqlQuery query;
	query.prepare("UPDATE assets SET name = ?, tags = ?, last_updated = datetime() WHERE guid = ?");
	query.addBindValue(name);
	query.addBindValue(tags);
	query.addBindValue(guid);

	executeAndCheckQuery(query, "updateAssetMetadata");
}

void Database::updateSceneGlobal(const QByteArray &sceneBlob, const QByteArray &thumbnail)
{
    QSqlQuery query;
    query.prepare("UPDATE projects SET scene = ?, last_written = datetime(), thumbnail = ? WHERE guid = ?");
    query.addBindValue(sceneBlob);
    query.addBindValue(thumbnail);
    query.addBindValue(Globals::project->getProjectGuid());

    executeAndCheckQuery(query, "updateSceneGlobal");
}

void Database::createExportScene(const QString &outTempFilePath)
{
    QSqlQuery query;
    query.prepare("SELECT name, scene, thumbnail, last_written, last_accessed, guid FROM " +
                  Constants::DB_PROJECTS_TABLE + " WHERE guid = ?");
    query.addBindValue(Globals::project->getProjectGuid());

    if (query.exec()) {
        query.next();
    } else {
        irisLog(
            "There was an error fetching a row to be exported " + query.lastError().text()
        );
    }

    auto sceneName  = query.value(0).toString();
    auto sceneBlob  = query.value(1).toByteArray();
    auto sceneThumb = query.value(2).toByteArray();
    auto sceneLastW = query.value(3).toDateTime();
    auto sceneLastA = query.value(4).toDateTime();
    auto sceneGuid  = query.value(5).toString();

    QSqlDatabase dbe = QSqlDatabase::addDatabase(Constants::DB_DRIVER, "myUniqueSQLITEConnection");
    dbe.setDatabaseName(QDir(outTempFilePath).filePath(Globals::project->getProjectName() + ".db"));
    dbe.open();

    QString schema = "CREATE TABLE IF NOT EXISTS " + Constants::DB_PROJECTS_TABLE + " ("
                     "    name              VARCHAR(64),"
                     "    thumbnail         BLOB,"
                     "    last_accessed     DATETIME,"
                     "    last_written      DATETIME,"
                     "    date_created      DATETIME DEFAULT CURRENT_TIMESTAMP,"
                     "    scene             BLOB,"
                     "    version           VARCHAR(8),"
                     "    description       TEXT,"
                     "    url               TEXT,"
                     "    guid              VARCHAR(32) PRIMARY KEY"
                     ")";

    QSqlQuery query2(dbe);
    query2.prepare(schema);
    executeAndCheckQuery(query2, "createExportGlobalDb");

    QSqlQuery query3(dbe);
    query3.prepare("INSERT INTO " + Constants::DB_PROJECTS_TABLE +
                   " (name, scene, thumbnail, last_written, last_accessed, guid)" +
                   " VALUES (:name, :scene, :thumbnail, :last_written, :last_accessed, :guid)");
    query3.bindValue(":name",           sceneName);
    query3.bindValue(":scene",          sceneBlob);
    query3.bindValue(":thumbnail",      sceneThumb);
    query3.bindValue(":last_written",   sceneLastW);
    query3.bindValue(":last_accessed",  sceneLastA);
    query3.bindValue(":guid",           sceneGuid);

    executeAndCheckQuery(query3, "insertSceneGlobal");

    dbe.close();
}

bool Database::importProject(const QString &inFilePath)
{
    QSqlDatabase dbe = QSqlDatabase::addDatabase(Constants::DB_DRIVER, "myUniqueSQLITEImportConnection");
    dbe.setDatabaseName(inFilePath + ".db");
    dbe.open();

    QSqlQuery query(dbe);
    query.prepare("SELECT name, scene, thumbnail, last_written, last_accessed, guid FROM " + Constants::DB_PROJECTS_TABLE);

    if (query.exec()) {
        query.next();
    } else {
        irisLog(
            "There was an error fetching a record to be imported " + query.lastError().text()
        );
    }

    auto sceneName  = query.value(0).toString();
    auto sceneBlob  = query.value(1).toByteArray();
    auto sceneThumb = query.value(2).toByteArray();
    auto sceneLastW = query.value(3).toDateTime();
    auto sceneLastA = query.value(4).toDateTime();
    auto sceneGuid  = query.value(5).toString();

    dbe.close();

    QSqlQuery query2;
    query2.prepare("SELECT EXISTS (SELECT 1 FROM " + Constants::DB_PROJECTS_TABLE + " WHERE guid = ? LIMIT 1)");
    query2.addBindValue(sceneGuid);

    bool exists = false;
    if (query2.exec()) {
        if (query2.first()) {
            exists = query2.record().value(0).toBool();
        }
    } else {
        irisLog("hasExistingProject query failed! " + query2.lastError().text());
    }

    if (exists) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(Q_NULLPTR,
                                      "Project Exists",
                                      "This project already exists, Replace it?",
                                      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (reply == QMessageBox::Yes) {
            QSqlQuery query31;
            query31.prepare("UPDATE " + Constants::DB_PROJECTS_TABLE + " SET scene = ?, thumbnail = ? WHERE guid = ?");
            query31.addBindValue(sceneBlob);
            query31.addBindValue(sceneThumb);
            query31.addBindValue(sceneGuid);

            executeAndCheckQuery(query31, "updateinsertSceneGlobal");

            Globals::project->setProjectGuid(sceneGuid);
            return true;
        } else if (reply == QMessageBox::No) {
            return false;
        }
    } else {
        QSqlQuery query3;
        query3.prepare("INSERT INTO " + Constants::DB_PROJECTS_TABLE                    +
                       " (name, scene, thumbnail, last_written, last_accessed, guid)"   +
                       " VALUES (:name, :scene, :thumbnail, :last_written, :last_accessed, :guid)");
        query3.bindValue(":name",           sceneName);
        query3.bindValue(":scene",          sceneBlob);
        query3.bindValue(":thumbnail",      sceneThumb);
        query3.bindValue(":last_written",   sceneLastW);
        query3.bindValue(":last_accessed",  sceneLastA);
        query3.bindValue(":guid",           sceneGuid);

        executeAndCheckQuery(query3, "insertSceneGlobal");

        Globals::project->setProjectGuid(sceneGuid);
        return true;
    }

    return false;
}

void Database::createExportNode(const ModelTypes &type, const QString &object_guid, const QString &outTempFilePath)
{
	QSqlDatabase datbaseConnection = QSqlDatabase::addDatabase(Constants::DB_DRIVER, "nodeExportSQLITEConnection");
	datbaseConnection.setDatabaseName(outTempFilePath);
	datbaseConnection.open();

	QString createAssetsTableSchema = 
		"CREATE TABLE IF NOT EXISTS assets ("
		"    guid              VARCHAR(32),"
		"	 type			   INTEGER,"
		"    name              VARCHAR(128),"
		"	 collection		   INTEGER,"
		"	 times_used		   INTEGER,"
		"    project_guid      VARCHAR(32),"
		"    date_created      DATETIME DEFAULT CURRENT_TIMESTAMP,"
		"    last_updated      DATETIME,"
		"	 author			   VARCHAR(128),"
		"    license		   VARCHAR(64),"
		"    hash              VARCHAR(16),"
		"    version           VARCHAR(8),"
		"    parent            VARCHAR(32),"
		"    tags			   BLOB,"
		"    properties        BLOB,"
		"    asset             BLOB,"
		"    thumbnail         BLOB"
		")";

	QSqlQuery createAssetsTableQuery(datbaseConnection);
	createAssetsTableQuery.prepare(createAssetsTableSchema);
	executeAndCheckQuery(createAssetsTableQuery, "createExportGlobalDb");

	QString dependenciesSchema =
		"CREATE TABLE IF NOT EXISTS dependencies ("
		"	 depender_type  INTEGER,"
		"	 dependee_type  INTEGER,"
		"    project_guid	VARCHAR(32),"
		"    depender		VARCHAR(32),"
		"    dependee		VARCHAR(32),"
		"    id				VARCHAR(32) PRIMARY KEY"
		")";

	QSqlQuery dependenciesCreateSchema(datbaseConnection);
	dependenciesCreateSchema.prepare(dependenciesSchema);
	executeAndCheckQuery(dependenciesCreateSchema, "createExportDependencies");

	QVector<AssetTileDataFull> assetList;
	for (const auto &asset : fetchAssetGUIDAndDependencies(object_guid)) {
		QSqlQuery selectAssetQuery;
		selectAssetQuery.prepare(
			"SELECT guid, type, name, collection, times_used, project_guid, date_created, last_updated, author, license, hash, version, parent, tags, properties, asset, thumbnail FROM assets WHERE guid = ?"
		);
		selectAssetQuery.addBindValue(asset);

		if (selectAssetQuery.exec()) {
			if (selectAssetQuery.first()) {
				AssetTileDataFull data;
				data.guid = selectAssetQuery.value(0).toString();
				data.type = selectAssetQuery.value(1).toInt();
				data.name = selectAssetQuery.value(2).toString();
				data.collection = selectAssetQuery.value(3).toInt();
				data.times_used = selectAssetQuery.value(4).toInt();
				data.project_guid = selectAssetQuery.value(5).toString();
				data.date_created = selectAssetQuery.value(6).toDateTime();
				data.last_updated = selectAssetQuery.value(7).toDateTime();
				data.author = selectAssetQuery.value(8).toString();
				data.license = selectAssetQuery.value(9).toString();
				data.hash = selectAssetQuery.value(10).toString();
				data.version = selectAssetQuery.value(11).toString();
				data.parent = selectAssetQuery.value(12).toString();
				data.tags = selectAssetQuery.value(13).toByteArray();
				data.properties = selectAssetQuery.value(14).toByteArray();
				data.asset = selectAssetQuery.value(15).toByteArray();
				data.thumbnail = selectAssetQuery.value(16).toByteArray();
				assetList.push_back(data);
			}
		}
		else {
			irisLog("There was an error fetching an asset " + selectAssetQuery.lastError().text());
		}
	}

	for (const auto &asset : assetList) {
		QSqlQuery insertExportAssetQuery(datbaseConnection);
		insertExportAssetQuery.prepare(
			"INSERT INTO assets"
			" (guid, type, name, collection, times_used, project_guid, date_created, last_updated, author,"
			" license, hash, version, parent, tags, properties, asset, thumbnail)"
			" VALUES(:guid, :type, :name, :collection, :times_used, :project_guid, :date_created, :last_updated, :author,"
			" :license, :hash, :version, :parent, :tags, :properties, :asset, :thumbnail)"
		);

		insertExportAssetQuery.bindValue(":guid", asset.guid);
		insertExportAssetQuery.bindValue(":type", asset.type);
		insertExportAssetQuery.bindValue(":name", asset.name);
		insertExportAssetQuery.bindValue(":collection", asset.collection);
		insertExportAssetQuery.bindValue(":times_used", asset.times_used);
		insertExportAssetQuery.bindValue(":project_guid", asset.project_guid);
		insertExportAssetQuery.bindValue(":date_created", asset.date_created);
		insertExportAssetQuery.bindValue(":last_updated", asset.last_updated);
		insertExportAssetQuery.bindValue(":author", asset.author);
		insertExportAssetQuery.bindValue(":license", asset.license);
		insertExportAssetQuery.bindValue(":hash", asset.hash);
		insertExportAssetQuery.bindValue(":version", asset.version);
		insertExportAssetQuery.bindValue(":parent", asset.parent);
		insertExportAssetQuery.bindValue(":tags", asset.tags);
		insertExportAssetQuery.bindValue(":properties", asset.properties);
		insertExportAssetQuery.bindValue(":asset", asset.asset);
		insertExportAssetQuery.bindValue(":thumbnail", asset.thumbnail);

		executeAndCheckQuery(insertExportAssetQuery, "insertExportAssetQuery");
	}

	QVector<DepRecord> dependenciesToExport;

	for (const auto &asset : assetList) {
		QSqlQuery selectDep;
		selectDep.prepare("SELECT depender_type, dependee_type, project_guid, depender, dependee, id FROM dependencies WHERE dependee = ? AND dependee_type = ? AND depender_type = ?");
		selectDep.addBindValue(asset.guid);
		selectDep.addBindValue(asset.type);
		selectDep.addBindValue(static_cast<int>(type));

		if (selectDep.exec()) {
			if (selectDep.first()) {
				auto ertype = selectDep.value(0).toInt();
				auto eetype = selectDep.value(1).toInt();
				auto project_guid = selectDep.value(2).toString();
				auto depender = selectDep.value(3).toString();
				auto dependee = selectDep.value(4).toString();
				auto id = selectDep.value(5).toString();

				DepRecord record;
				record.ertype = ertype;
				record.eetype = eetype;
				record.project_guid = project_guid;
				record.depender = depender;
				record.dependee = dependee;
				record.id = id;

				dependenciesToExport.append(record);
			}
		}
		else {
			irisLog("There was an error fetching a dependency" + selectDep.lastError().text());
		}
	}

	QStringList assetDependencies;

	for (const auto &dep : dependenciesToExport) {
		QSqlQuery exportDep(datbaseConnection);
		exportDep.prepare(
			"INSERT INTO dependencies (depender_type, dependee_type, project_guid, depender, dependee, id) "
            "VALUES (:depender_type, :dependee_type, :project_guid, :depender, :dependee, :id)"
		);

		exportDep.bindValue(":depender_type", dep.ertype);
		exportDep.bindValue(":dependee_type", dep.eetype);
		exportDep.bindValue(":project_guid", dep.project_guid);
		exportDep.bindValue(":depender", dep.depender);
		exportDep.bindValue(":dependee", dep.dependee);
		exportDep.bindValue(":id", dep.id);

		executeAndCheckQuery(exportDep, "exportDep");
	}

	datbaseConnection.close();
	QSqlDatabase::removeDatabase("nodeExportSQLITEConnection");
}

bool Database::checkIfRecordExists(const QString & record, const QVariant &value, const QString & table)
{
	QSqlQuery query;
	query.prepare("SELECT EXISTS (SELECT 1 FROM assets WHERE guid = ? LIMIT 1)");
	query.addBindValue(value);

	if (query.exec()) {
		if (query.first()) return query.value(0).toBool();
	}
	else {
		irisLog("There was an error fetching a record" + query.lastError().text());
	}

	return false;
}

QStringList Database::fetchFolderNameByParent(const QString &guid)
{
	QSqlQuery query;
	query.prepare("SELECT name FROM folders WHERE parent = ?");
	query.addBindValue(guid);
	executeAndCheckQuery(query, "fetchFolderNameByParent");

	QStringList folders;
	while (query.next()) {
		QSqlRecord record = query.record();
		folders.append(record.value(0).toString());
	}

	return folders;
}

QStringList Database::fetchFolderAndChildFolders(const QString &guid)
{
	std::function<void(QStringList&, const QString&)> fetchFolders
		= [&](QStringList &folders, const QString &guid) -> void
	{
		QSqlQuery query;
		query.prepare("SELECT guid FROM folders WHERE parent = ?");
		query.addBindValue(guid);
		executeAndCheckQuery(query, "fetchFolderAndDependencies");

		QStringList subFolders;
		while (query.next()) {
			QSqlRecord record = query.record();
			subFolders.append(record.value(0).toString());
			folders.append(record.value(0).toString());
		}

		for (const QString &folder : subFolders) {
			fetchFolders(folders, folder);
		}
	};

	QStringList folders;
	fetchFolders(folders, guid);
	folders.append(guid);

	return folders;
}

QStringList Database::fetchChildFolderAssets(const QString &guid)
{
	QSqlQuery query;
	query.prepare("SELECT guid FROM assets WHERE parent = ?");
	query.addBindValue(guid);
	executeAndCheckQuery(query, "fetchChildFolderAssets");

	QStringList assets;
	while (query.next()) {
		QString data;
		QSqlRecord record = query.record();
		if (Constants::IMAGE_EXTS.contains(QFileInfo(record.value(0).toString()).suffix().toLower())) {
			data = QDir("Textures").filePath(record.value(0).toString());
		}
		else if (Constants::MODEL_EXTS.contains(QFileInfo(record.value(0).toString()).suffix().toLower())) {
			data = QDir("Models").filePath(record.value(0).toString());
		}
		else {
			data = record.value(0).toString();
		}

		assets.append(data);
	}

	return assets;
}

bool Database::deleteDependency(const QString & dependee)
{
	QSqlQuery query;
	query.prepare("DELETE FROM dependencies WHERE dependee = ?");
	query.addBindValue(dependee);
	return executeAndCheckQuery(query, "deleteDependency");
}

QStringList Database::fetchAssetDependenciesByType(const QString & guid, const ModelTypes &type)
{
    QSqlQuery query;
    query.prepare(
        "SELECT assets.guid FROM dependencies "
        "INNER JOIN assets ON dependencies.dependee = assets.guid "
        "WHERE depender = ? AND depender_type = ?");
    query.addBindValue(guid);
    query.addBindValue(static_cast<int>(type));
    executeAndCheckQuery(query, "fetchAssetDependenciesByType");

    QStringList dependencies;
    while (query.next()) {
        QSqlRecord record = query.record();
        dependencies.append(record.value(0).toString());
    }
    return dependencies;
}

QStringList Database::fetchAssetAndDependencies(const QString &guid)
{
	QSqlQuery query;
	query.prepare(
		"SELECT assets.name FROM dependencies "
		"INNER JOIN assets ON dependencies.dependee = assets.guid "
		"WHERE depender = ?"
	);
	query.addBindValue(guid);
	executeAndCheckQuery(query, "fetchAssetAndDependencies");

	auto prependPath = [](const QString &value) {
		QString fileSuffix = QFileInfo(value).suffix().toLower();
		if (Constants::IMAGE_EXTS.contains(fileSuffix)) {
			return QDir("Textures").filePath(value);
		}
		else if (Constants::MODEL_EXTS.contains(fileSuffix)) {
			return QDir("Models").filePath(value);
		}
		else if (fileSuffix == Constants::SHADER_EXT) {
			return QDir("Shaders").filePath(value);
		}
		else if (fileSuffix == Constants::MATERIAL_EXT) {
			return QDir("Materials").filePath(value);
		}
        else if (Constants::WHITELIST.contains(fileSuffix)) {
            return QDir("Files").filePath(value);
        }
		else {
			return value;
		}
	};

	QStringList dependencies;
	dependencies.append(prependPath(fetchAsset(guid).name));
	while (query.next()) {
		QSqlRecord record = query.record();
		dependencies.append(prependPath(record.value(0).toString()));
	}

	for (int i = 0; i < dependencies.size(); ++i) {
		if (QFileInfo(dependencies[i]).suffix().isEmpty()) {
			dependencies.removeAt(i);
		}
	}

	return dependencies;
}

QStringList Database::fetchAssetGUIDAndDependencies(const QString &guid, bool appendSelf)
{
	QSqlQuery query;
	query.prepare(
        "SELECT assets.guid FROM dependencies INNER JOIN assets ON "
        "dependencies.dependee = assets.guid WHERE depender = ?"
    );
	query.addBindValue(guid);
	executeAndCheckQuery(query, "fetchAssetGUIDAndDependencies");

	QStringList dependencies;
    if (appendSelf) dependencies.append(guid);
	while (query.next()) {
		QSqlRecord record = query.record();
		dependencies.append(record.value(0).toString());
	}

	//for (int i = 0; i < dependencies.size(); ++i) {
	//	if (QFileInfo(dependencies[i]).suffix().isEmpty()) {
	//		dependencies.removeAt(i);
	//	}
	//}

	return dependencies;
}

QStringList Database::deleteFolderAndDependencies(const QString &guid)
{
	QStringList files;

	// Get all child folders
	for (const auto &folder : fetchFolderAndChildFolders(guid)) {
		// For every folder, fetch assets inside
		for (const auto &asset : fetchChildFolderAssets(folder)) {
			// For every asset, find their dependencies
			for (const auto &dep : fetchAssetAndDependencies(asset)) {
				files.append(dep);
			}

			deleteAsset(asset);
			deleteDependency(asset);
		}

		deleteFolder(folder);
	}

	for (int i = 0; i < files.size(); ++i) {
		if (QFileInfo(files[i]).suffix().isEmpty()) {
			files.removeAt(i);
		}
	}

	files.removeDuplicates();
	return files;
}

QStringList Database::deleteAssetAndDependencies(const QString & guid)
{
	QStringList files;

	// For every asset, find their dependencies
	for (const auto &asset : fetchAssetGUIDAndDependencies(guid)) {
		for (const auto &dep : fetchAssetAndDependencies(asset)) {
			files.append(dep);
		}

		deleteAsset(asset);
		deleteDependency(asset);
	}

    for (int i = 0; i < files.size(); ++i) {
	    if (QFileInfo(files[i]).suffix().isEmpty()) {
		    files.removeAt(i);
	    }
    }

    files.removeDuplicates();
    return files;
}

QString Database::fetchAssetGUIDByName(const QString & name)
{
	QSqlQuery query;
	query.prepare("SELECT guid FROM assets WHERE name = ? AND project_guid = ?");
	query.addBindValue(name);
	query.addBindValue(Globals::project->getProjectGuid());

	if (query.exec()) {
		if (query.first()) {
			return query.value(0).toString();
		}
	}
	else {
		irisLog(
			"There was an error fetching a guid for an asset (" + name + ")" + query.lastError().text()
		);
	}

	return QString();
}

QString Database::fetchObjectMesh(const QString &guid, const int ertype, const int eetype)
{
	QSqlQuery query;
	query.prepare("SELECT dependee FROM dependencies WHERE depender = ? AND depender_type = ? AND dependee_type = ?");
	query.addBindValue(guid);
	query.addBindValue(ertype);
	query.addBindValue(eetype);

	if (query.exec()) {
		if (query.first()) {
			return query.value(0).toString();
		}
	}
	else {
		irisLog(
			"There was an error fetching a guid" + query.lastError().text()
		);
	}

	return QString();
}

QString Database::fetchMeshObject(const QString &guid, const int ertype, const int eetype)
{
	QSqlQuery query;
	query.prepare("SELECT depender FROM dependencies WHERE dependee = ? AND depender_type = ? AND dependee_type == ?");
	query.addBindValue(guid);
	query.addBindValue(ertype);
	query.addBindValue(eetype);

	if (query.exec()) {
		if (query.first()) {
			return query.value(0).toString();
		}
	}
	else {
		irisLog(
			"There was an error fetching a guid" + query.lastError().text()
		);
	}

	return QString();
}

QString Database::importAsset(
	const ModelTypes &jafType,
	const QString & pathToDb,
	const QMap<QString, QString>& newNames,
	const QString &parent)
{
	QSqlDatabase dbe = QSqlDatabase::addDatabase(Constants::DB_DRIVER, "newSqlImportConnection");
	dbe.setDatabaseName(pathToDb);
	dbe.open();

	QSqlQuery selectAssetQuery(dbe);
	selectAssetQuery.prepare("SELECT guid, type, name, collection, times_used, project_guid, date_created, last_updated, author, license, hash, version, parent, tags, properties, asset, thumbnail FROM assets");
	executeAndCheckQuery(selectAssetQuery, "fetchImportAssets");

	QMap<QString, QString> assetGuids; /* old x new guid */

	const QString guidToReturn = GUIDManager::generateGUID();

	QVector<AssetTileDataFull> assetsToImport;

	while (selectAssetQuery.next()) {
		AssetTileDataFull data;
		QSqlRecord record = selectAssetQuery.record();

		for (int i = 0; i < record.count(); i++) {
			if (selectAssetQuery.value(1).toInt() == static_cast<int>(ModelTypes::Object) ||
				selectAssetQuery.value(1).toInt() == static_cast<int>(ModelTypes::Material)) {
				data.guid = guidToReturn;
				assetGuids.insert(record.value(0).toString(), guidToReturn);
			}
			else {
				QString guid = GUIDManager::generateGUID();
				assetGuids.insert(record.value(0).toString(), guid);
				data.guid = guid;
			}

			data.type = record.value(1).toInt();

			// If we find a file with the same name, rename it (may or may not change)
			for (const auto &name : newNames) {
				if (!newNames.value(record.value(2).toString()).isEmpty()) {
					data.name = newNames.value(record.value(2).toString());
				}
				else data.name = record.value(2).toString();
			}

			data.collection = record.value(3).toInt();
			data.times_used = record.value(4).toInt();
			data.project_guid = Globals::project->getProjectGuid();
			data.date_created = record.value(6).toDateTime();
			data.last_updated = record.value(7).toDateTime();
			data.author = record.value(8).toString();
			data.license = record.value(9).toString();
			data.hash = record.value(10).toString();
			data.version = record.value(11).toString();
			data.parent = parent;
			data.tags = record.value(13).toByteArray();
			data.properties = record.value(14).toByteArray();
			data.asset = record.value(15).toByteArray();
			data.thumbnail = record.value(16).toByteArray();
		}

		assetsToImport.push_back(data);
	}

	if (jafType == ModelTypes::Object) {
		for (auto &asset : assetsToImport) {
			if (asset.type == static_cast<int>(ModelTypes::Object)) {
				auto doc = QJsonDocument::fromBinaryData(asset.asset);
				QString docToString = doc.toJson(QJsonDocument::Compact);

				QMapIterator<QString, QString> i(assetGuids);
				while (i.hasNext()) {
					i.next();
					docToString.replace(i.key(), i.value());
				}

				QJsonDocument updatedDoc = QJsonDocument::fromJson(docToString.toUtf8());
				asset.asset = updatedDoc.toBinaryData();
			}
		}
	}

    if (jafType == ModelTypes::Material) {
        for (auto &asset : assetsToImport) {
            if (asset.type == static_cast<int>(ModelTypes::Material)) {
                auto doc = QJsonDocument::fromBinaryData(asset.asset);
                QString docToString = doc.toJson(QJsonDocument::Compact);

                QMapIterator<QString, QString> i(assetGuids);
                while (i.hasNext()) {
                    i.next();
                    docToString.replace(i.key(), i.value());
                }

                QJsonDocument updatedDoc = QJsonDocument::fromJson(docToString.toUtf8());
                asset.asset = updatedDoc.toBinaryData();
            }
        }
    }

	QSqlQuery selectDepQuery(dbe);
	selectDepQuery.prepare("SELECT depender_type, dependee_type, project_guid, depender, dependee, id FROM dependencies");
	executeAndCheckQuery(selectDepQuery, "fetchImportDeps");

	QVector<DepRecord> depsToImport;
	while (selectDepQuery.next()) {
		DepRecord data;
		QSqlRecord record = selectDepQuery.record();
		for (int i = 0; i < record.count(); i++) {
			data.ertype = record.value(0).toInt();
			data.eetype = record.value(1).toInt();
			data.project_guid = Globals::project->getProjectGuid();
			data.depender = assetGuids.value(record.value(3).toString());
			data.dependee = assetGuids.value(record.value(4).toString());
			data.id = GUIDManager::generateGUID();
		}

		depsToImport.push_back(data);
	}

	for (const auto &asset : assetsToImport) {
		QSqlQuery insertAssetQuery;
		insertAssetQuery.prepare(
			"INSERT INTO assets"
			" (guid, type, name, collection, times_used, project_guid, date_created, last_updated, author,"
			" license, hash, version, parent, tags, properties, asset, thumbnail)"
			" VALUES(:guid, :type, :name, :collection, :times_used, :project_guid, :date_created, :last_updated, :author,"
			" :license, :hash, :version, :parent, :tags, :properties, :asset, :thumbnail)"
		);

		insertAssetQuery.bindValue(":guid", asset.guid);
		insertAssetQuery.bindValue(":type", asset.type);
		insertAssetQuery.bindValue(":name", asset.name);
		insertAssetQuery.bindValue(":collection", asset.collection);
		insertAssetQuery.bindValue(":times_used", asset.times_used);
		insertAssetQuery.bindValue(":project_guid", asset.project_guid);
		insertAssetQuery.bindValue(":date_created", asset.date_created);
		insertAssetQuery.bindValue(":last_updated", asset.last_updated);
		insertAssetQuery.bindValue(":author", asset.author);
		insertAssetQuery.bindValue(":license", asset.license);
		insertAssetQuery.bindValue(":hash", asset.hash);
		insertAssetQuery.bindValue(":version", asset.version);
		insertAssetQuery.bindValue(":parent", asset.parent);
		insertAssetQuery.bindValue(":tags", asset.tags);
		insertAssetQuery.bindValue(":properties", asset.properties);
		insertAssetQuery.bindValue(":asset", asset.asset);
		insertAssetQuery.bindValue(":thumbnail", asset.thumbnail);

		executeAndCheckQuery(insertAssetQuery, "insertAssetQuery");
	}

	QSqlQuery exportDep;
	exportDep.prepare(
		"INSERT INTO dependencies (depender_type, dependee_type, project_guid, depender, dependee, id) "
        "VALUES (:depender_type, :dependee_type, :project_guid, :depender, :dependee, :id)"
	);

	for (const auto &dep : depsToImport) {
		exportDep.bindValue(":depender_type", dep.ertype);
		exportDep.bindValue(":dependee_type", dep.eetype);
		exportDep.bindValue(":project_guid", dep.project_guid);
		exportDep.bindValue(":depender", dep.depender);
		exportDep.bindValue(":dependee", dep.dependee);
		exportDep.bindValue(":id", dep.id);

		executeAndCheckQuery(exportDep, "exportDep");
	}

	dbe.close();

	return guidToReturn;
}

QString Database::importJafAssetModel(const ModelTypes &jafType, const QString & pathToDb)
{
    QSqlDatabase dbe = QSqlDatabase::addDatabase(Constants::DB_DRIVER, "newSqlImportConnection");
    dbe.setDatabaseName(pathToDb);
    dbe.open();

    QSqlQuery selectAssetQuery(dbe);
    selectAssetQuery.prepare("SELECT guid, type, name, collection, times_used, project_guid, date_created, last_updated, author, license, hash, version, parent, tags, properties, asset, thumbnail FROM assets");
    executeAndCheckQuery(selectAssetQuery, "fetchImportAssets");

    QMap<QString, QString> assetGuids; /* old x new guid */

    const QString guidToReturn = GUIDManager::generateGUID();

    QVector<AssetTileDataFull> assetsToImport;

    while (selectAssetQuery.next()) {
        AssetTileDataFull data;
        QSqlRecord record = selectAssetQuery.record();

        for (int i = 0; i < record.count(); i++) {
            if (selectAssetQuery.value(1).toInt() == static_cast<int>(ModelTypes::Object) ||
                selectAssetQuery.value(1).toInt() == static_cast<int>(ModelTypes::Material)) {
                data.guid = guidToReturn;
                assetGuids.insert(record.value(0).toString(), guidToReturn);
            }
            else {
                QString guid = GUIDManager::generateGUID();
                assetGuids.insert(record.value(0).toString(), guid);
                data.guid = guid;
            }

            data.type = record.value(1).toInt();
            data.name = record.value(2).toString();
            data.collection = record.value(3).toInt();
            data.times_used = record.value(4).toInt();
            data.project_guid = record.value(5).toString();
            data.date_created = record.value(6).toDateTime();
            data.last_updated = record.value(7).toDateTime();
            data.author = record.value(8).toString();
            data.license = record.value(9).toString();
            data.hash = record.value(10).toString();
            data.version = record.value(11).toString();
            data.parent = QString();
            data.tags = record.value(13).toByteArray();
            data.properties = record.value(14).toByteArray();
            data.asset = record.value(15).toByteArray();
            data.thumbnail = record.value(16).toByteArray();
        }

        assetsToImport.push_back(data);
    }

    if (jafType == ModelTypes::Object) {
        for (auto &asset : assetsToImport) {
            if (asset.type == static_cast<int>(ModelTypes::Object)) {
                auto doc = QJsonDocument::fromBinaryData(asset.asset);
                QString docToString = doc.toJson(QJsonDocument::Compact);

                QMapIterator<QString, QString> i(assetGuids);
                while (i.hasNext()) {
                    i.next();
                    docToString.replace(i.key(), i.value());
                }

                QJsonDocument updatedDoc = QJsonDocument::fromJson(docToString.toUtf8());
                asset.asset = updatedDoc.toBinaryData();
            }
        }
    }

    if (jafType == ModelTypes::Material) {
        for (auto &asset : assetsToImport) {
            if (asset.type == static_cast<int>(ModelTypes::Material)) {
                auto doc = QJsonDocument::fromBinaryData(asset.asset);
                QString docToString = doc.toJson(QJsonDocument::Compact);

                QMapIterator<QString, QString> i(assetGuids);
                while (i.hasNext()) {
                    i.next();
                    docToString.replace(i.key(), i.value());
                }

                QJsonDocument updatedDoc = QJsonDocument::fromJson(docToString.toUtf8());
                asset.asset = updatedDoc.toBinaryData();
            }
        }
    }

    QSqlQuery selectDepQuery(dbe);
    selectDepQuery.prepare("SELECT depender_type, dependee_type, project_guid, depender, dependee, id FROM dependencies");
    executeAndCheckQuery(selectDepQuery, "fetchImportDeps");

    QVector<DepRecord> depsToImport;
    while (selectDepQuery.next()) {
        DepRecord data;
        QSqlRecord record = selectDepQuery.record();
        for (int i = 0; i < record.count(); i++) {
            data.ertype = record.value(0).toInt();
            data.eetype = record.value(1).toInt();
            data.project_guid = record.value(2).toString();
            data.depender = assetGuids.value(record.value(3).toString());
            data.dependee = assetGuids.value(record.value(4).toString());
            data.id = GUIDManager::generateGUID();
        }

        depsToImport.push_back(data);
    }

    for (const auto &asset : assetsToImport) {
        QSqlQuery insertAssetQuery;
        insertAssetQuery.prepare(
            "INSERT INTO assets"
            " (guid, type, name, collection, times_used, project_guid, date_created, last_updated, author,"
            " license, hash, version, parent, tags, properties, asset, thumbnail)"
            " VALUES(:guid, :type, :name, :collection, :times_used, :project_guid, :date_created, :last_updated, :author,"
            " :license, :hash, :version, :parent, :tags, :properties, :asset, :thumbnail)"
        );

        insertAssetQuery.bindValue(":guid", asset.guid);
        insertAssetQuery.bindValue(":type", asset.type);
        insertAssetQuery.bindValue(":name", asset.name);
        insertAssetQuery.bindValue(":collection", asset.collection);
        insertAssetQuery.bindValue(":times_used", asset.times_used);
        insertAssetQuery.bindValue(":project_guid", asset.project_guid);
        insertAssetQuery.bindValue(":date_created", asset.date_created);
        insertAssetQuery.bindValue(":last_updated", asset.last_updated);
        insertAssetQuery.bindValue(":author", asset.author);
        insertAssetQuery.bindValue(":license", asset.license);
        insertAssetQuery.bindValue(":hash", asset.hash);
        insertAssetQuery.bindValue(":version", asset.version);
        insertAssetQuery.bindValue(":parent", asset.parent);
        insertAssetQuery.bindValue(":tags", asset.tags);
        insertAssetQuery.bindValue(":properties", asset.properties);
        insertAssetQuery.bindValue(":asset", asset.asset);
        insertAssetQuery.bindValue(":thumbnail", asset.thumbnail);

        executeAndCheckQuery(insertAssetQuery, "insertAssetQuery");
    }

    QSqlQuery exportDep;
    exportDep.prepare(
        "INSERT INTO dependencies (depender_type, dependee_type, project_guid, depender, dependee, id) "
        "VALUES (:depender_type, :dependee_type, :project_guid, :depender, :dependee, :id)"
    );

    for (const auto &dep : depsToImport) {
        exportDep.bindValue(":depender_type", dep.ertype);
        exportDep.bindValue(":dependee_type", dep.eetype);
        exportDep.bindValue(":project_guid", dep.project_guid);
        exportDep.bindValue(":depender", dep.depender);
        exportDep.bindValue(":dependee", dep.dependee);
        exportDep.bindValue(":id", dep.id);

        executeAndCheckQuery(exportDep, "exportDep");
    }

    dbe.close();

    return guidToReturn;
}

bool Database::renameFolder(const QString &guid, const QString &newName)
{
	QSqlQuery query;
	query.prepare("UPDATE folders SET name = ? WHERE guid = ?");
	query.addBindValue(newName);
	query.addBindValue(guid);

	return executeAndCheckQuery(query, "renameFolder");
}

bool Database::renameAsset(const QString &guid, const QString &newName)
{
	QSqlQuery query;
	query.prepare("UPDATE assets SET name = ? WHERE guid = ?");
	query.addBindValue(newName);
	query.addBindValue(guid);

	return executeAndCheckQuery(query, "renameAsset");
}