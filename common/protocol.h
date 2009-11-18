#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QString>
#include <QMap>
#include <QHash>
#include <QObject>
#include <QDebug>
#include "protocol_item_ids.h"
#include "protocol_datastructures.h"

class QXmlStreamReader;
class QXmlStreamWriter;
class QXmlStreamAttributes;

class ProtocolResponse;
class DeckList;

enum ItemId {
	ItemId_Command_DeckUpload = ItemId_Other + 1,
	ItemId_Event_ListChatChannels = ItemId_Other + 2,
	ItemId_Event_ChatListPlayers = ItemId_Other + 3,
	ItemId_Event_ListGames = ItemId_Other + 4,
	ItemId_Response_DeckList = ItemId_Other + 5,
	ItemId_Response_DeckDownload = ItemId_Other + 6,
	ItemId_Response_DeckUpload = ItemId_Other + 7
};

class ProtocolItem : public QObject {
	Q_OBJECT
private:
	QString currentElementText;
protected:
	typedef ProtocolItem *(*NewItemFunction)();
	static QHash<QString, NewItemFunction> itemNameHash;
	
	QString itemName;
	QMap<QString, QString> parameters;
	void setParameter(const QString &name, const QString &value) { parameters[name] = value; }
	void setParameter(const QString &name, bool value) { parameters[name] = (value ? "1" : "0"); }
	void setParameter(const QString &name, int value) { parameters[name] = QString::number(value); }
	virtual void extractParameters() { }
	virtual QString getItemType() const = 0;
	
	virtual bool readElement(QXmlStreamReader * /*xml*/) { return false; }
	virtual void writeElement(QXmlStreamWriter * /*xml*/) { }
private:
	static void initializeHashAuto();
public:
	static const int protocolVersion = 4;
	virtual int getItemId() const = 0;
	ProtocolItem(const QString &_itemName);
	static void initializeHash();
	static ProtocolItem *getNewItem(const QString &name);
	bool read(QXmlStreamReader *xml);
	void write(QXmlStreamWriter *xml);
};

// ----------------
// --- COMMANDS ---
// ----------------

class Command : public ProtocolItem {
	Q_OBJECT
signals:
	void finished(ProtocolResponse *response);
	void finished(ResponseCode response);
private:
	int cmdId;
	int ticks;
	static int lastCmdId;
protected:
	QString getItemType() const { return "cmd"; }
	void extractParameters();
public:
	Command(const QString &_itemName = QString(), int _cmdId = -1);
	int getCmdId() const { return cmdId; }
	int tick() { return ++ticks; }
	void processResponse(ProtocolResponse *response);
};

class InvalidCommand : public Command {
	Q_OBJECT
public:
	InvalidCommand() : Command() { }
	int getItemId() const { return ItemId_Other; }
};

class ChatCommand : public Command {
	Q_OBJECT
private:
	QString channel;
protected:
	void extractParameters()
	{
		Command::extractParameters();
		channel = parameters["channel"];
	}
public:
	ChatCommand(const QString &_cmdName, const QString &_channel)
		: Command(_cmdName), channel(_channel)
	{
		setParameter("channel", channel);
	}
	QString getChannel() const { return channel; }
};

class GameCommand : public Command {
	Q_OBJECT
private:
	int gameId;
protected:
	void extractParameters()
	{
		Command::extractParameters();
		gameId = parameters["game_id"].toInt();
	}
public:
	GameCommand(const QString &_cmdName, int _gameId)
		: Command(_cmdName), gameId(_gameId)
	{
		setParameter("game_id", gameId);
	}
	int getGameId() const { return gameId; }
};

class Command_DeckUpload : public Command {
	Q_OBJECT
private:
	DeckList *deck;
	QString path;
	bool readFinished;
protected:
	void extractParameters();
	bool readElement(QXmlStreamReader *xml);
	void writeElement(QXmlStreamWriter *xml);
public:
	Command_DeckUpload(int _cmdId = -1, DeckList *_deck = 0, const QString &_path = QString());
	~Command_DeckUpload();
	static ProtocolItem *newItem() { return new Command_DeckUpload; }
	int getItemId() const { return ItemId_Command_DeckUpload; }
	DeckList *getDeck() const { return deck; }
	QString getPath() const { return path; }
};

// -----------------
// --- RESPONSES ---
// -----------------

class ProtocolResponse : public ProtocolItem {
	Q_OBJECT
private:
	int cmdId;
	ResponseCode responseCode;
	static QHash<QString, ResponseCode> responseHash;
protected:
	QString getItemType() const { return "resp"; }
	void extractParameters();
public:
	ProtocolResponse(int _cmdId = -1, ResponseCode _responseCode = RespOk, const QString &_itemName = QString());
	int getItemId() const { return ItemId_Other; }
	static void initializeHash();
	static ProtocolItem *newItem() { return new ProtocolResponse; }
	int getCmdId() const { return cmdId; }
	ResponseCode getResponseCode() const { return responseCode; }
};

class Response_DeckList : public ProtocolResponse {
	Q_OBJECT
public:
	class TreeItem {
	protected:
		QString name;
		int id;
	public:
		TreeItem(const QString &_name, int _id) : name(_name), id(_id) { }
		QString getName() const { return name; }
		int getId() const { return id; }
		virtual bool readElement(QXmlStreamReader *xml) = 0;
		virtual void writeElement(QXmlStreamWriter *xml) = 0;
	};
	class File : public TreeItem {
	public:
		File(const QString &_name, int _id) : TreeItem(_name, _id) { }
		bool readElement(QXmlStreamReader *xml);
		void writeElement(QXmlStreamWriter *xml);
	};
	class Directory : public TreeItem, public QList<TreeItem *> {
	private:
		TreeItem *currentItem;
	public:
		Directory(const QString &_name = QString(), int _id = 0) : TreeItem(_name, _id), currentItem(0) { }
		~Directory();
		bool readElement(QXmlStreamReader *xml);
		void writeElement(QXmlStreamWriter *xml);
	};
private:
	Directory *root;
	bool readFinished;
protected:
	bool readElement(QXmlStreamReader *xml);
	void writeElement(QXmlStreamWriter *xml);
public:
	Response_DeckList(int _cmdId = -1, ResponseCode _responseCode = RespOk, Directory *_root = 0);
	~Response_DeckList();
	int getItemId() const { return ItemId_Response_DeckList; }
	static ProtocolItem *newItem() { return new Response_DeckList; }
	Directory *getRoot() const { return root; }
};

class Response_DeckDownload : public ProtocolResponse {
	Q_OBJECT
private:
	DeckList *deck;
	bool readFinished;
protected:
	bool readElement(QXmlStreamReader *xml);
	void writeElement(QXmlStreamWriter *xml);
public:
	Response_DeckDownload(int _cmdId = -1, ResponseCode _responseCode = RespOk, DeckList *_deck = 0);
	~Response_DeckDownload();
	int getItemId() const { return ItemId_Response_DeckDownload; }
	static ProtocolItem *newItem() { return new Response_DeckDownload; }
	DeckList *getDeck() const { return deck; }
};

class Response_DeckUpload : public ProtocolResponse {
	Q_OBJECT
private:
	int deckId;
protected:
	void extractParameters();
public:
	Response_DeckUpload(int _cmdId = -1, ResponseCode _responseCode = RespOk, int _deckId = -1);
	int getItemId() const { return ItemId_Response_DeckUpload; }
	static ProtocolItem *newItem() { return new Response_DeckUpload; }
	int getDeckId() const { return deckId; }
};

// --------------
// --- EVENTS ---
// --------------

class GenericEvent : public ProtocolItem {
	Q_OBJECT
protected:
	QString getItemType() const { return "generic_event"; }
public:
	GenericEvent(const QString &_eventName);
};

class GameEvent : public ProtocolItem {
	Q_OBJECT
private:
	int gameId;
	int playerId;
protected:
	QString getItemType() const { return "game_event"; }
	void extractParameters();
public:
	GameEvent(const QString &_eventName, int _gameId, int _playerId);
	int getGameId() const { return gameId; }
	int getPlayerId() const { return playerId; }
	void setGameId(int _gameId) { gameId = _gameId; }
};

class ChatEvent : public ProtocolItem {
	Q_OBJECT
private:
	QString channel;
protected:
	QString getItemType() const { return "chat_event"; }
	void extractParameters();
public:
	ChatEvent(const QString &_eventName, const QString &_channel);
	QString getChannel() const { return channel; }
};

class Event_ListChatChannels : public GenericEvent {
	Q_OBJECT
private:
	QList<ServerChatChannelInfo> channelList;
public:
	Event_ListChatChannels() : GenericEvent("list_chat_channels") { }
	int getItemId() const { return ItemId_Event_ListChatChannels; }
	static ProtocolItem *newItem() { return new Event_ListChatChannels; }
	void addChannel(const QString &_name, const QString &_description, int _playerCount, bool _autoJoin)
	{
		channelList.append(ServerChatChannelInfo(_name, _description, _playerCount, _autoJoin));
	}
	const QList<ServerChatChannelInfo> &getChannelList() const { return channelList; }

	bool readElement(QXmlStreamReader *xml);
	void writeElement(QXmlStreamWriter *xml);
};

class Event_ChatListPlayers : public ChatEvent {
	Q_OBJECT
private:
	QList<ServerPlayerInfo> playerList;
public:
	Event_ChatListPlayers(const QString &_channel = QString()) : ChatEvent("chat_list_players", _channel) { }
	int getItemId() const { return ItemId_Event_ChatListPlayers; }
	static ProtocolItem *newItem() { return new Event_ChatListPlayers; }
	void addPlayer(const QString &_name)
	{
		playerList.append(ServerPlayerInfo(_name));
	}
	const QList<ServerPlayerInfo> &getPlayerList() const { return playerList; }

	bool readElement(QXmlStreamReader *xml);
	void writeElement(QXmlStreamWriter *xml);
};

class Event_ListGames : public GenericEvent {
	Q_OBJECT
private:
	QList<ServerGameInfo> gameList;
public:
	Event_ListGames() : GenericEvent("list_games") { }
	int getItemId() const { return ItemId_Event_ListGames; }
	static ProtocolItem *newItem() { return new Event_ListGames; }
	void addGame(int _gameId, const QString &_description, bool _hasPassword, int _playerCount, int _maxPlayers, const QString &_creatorName, bool _spectatorsAllowed, int _spectatorCount)
	{
		gameList.append(ServerGameInfo(_gameId, _description, _hasPassword, _playerCount, _maxPlayers, _creatorName, _spectatorsAllowed, _spectatorCount));
	}
	const QList<ServerGameInfo> &getGameList() const { return gameList; }

	bool readElement(QXmlStreamReader *xml);
	void writeElement(QXmlStreamWriter *xml);
};

#endif