#include "sqlite_repository.h"

#include <algorithm>
#include <iostream>
#include <random>

SqliteRepository::SqliteRepository(std::unique_ptr<SQLite::Database> db,
                                   std::vector<User> inital_users)
    : db_(std::move(db)), initial_users_(std::move(inital_users)) {}

std::expected<void, std::string> SqliteRepository::init() {
    try {
        db_->exec(
            "CREATE TABLE IF NOT EXISTS user(user_id INTEGER PRIMARY KEY, "
            "telegram_id "
            "INTEGER UNIQUE, first_name TEXT, surname TEXT, second_name "
            "TEXT, admin INTEGER)");

        db_->exec(
            "CREATE TABLE IF NOT EXISTS list(list_id INTEGER PRIMARY KEY, "
            "name TEXT UNIQUE)");

        db_->exec(
            "CREATE TABLE IF NOT EXISTS list_user(list_user_id INTEGER PRIMARY KEY, "
            "list_id INTEGER, user_id INTEGER, list_user_order INTEGER)");

        for (const User& user : initial_users_) {
            auto add_result = tryAddUser(user);

            if (!add_result) {
                std::cerr << "Could not add user: " << user.first_name << " "
                          << user.surname << " | " << add_result.error()
                          << " | skipping..." << std::endl;
                continue;
            }
        }

        return {};
    }
    catch (const SQLite::Exception& e) {
        return std::unexpected(e.what());
    }
}

std::expected<std::vector<User>, std::string> SqliteRepository::tryGetAllUsers() {
    SQLite::Statement query(*db_, "SELECT * FROM user");

    std::vector<User> users;

    try {
        while (query.executeStep()) {
            users.emplace_back(query.getColumn(0), query.getColumn(1), query.getColumn(2),
                               query.getColumn(3), query.getColumn(4),
                               (int64_t)query.getColumn(5));
        }

        if (users.empty()) {
            return std::unexpected("No users present");
        }

        return users;
    }
    catch (const std::exception& e) {
        return std::unexpected(e.what());
    }
}

std::expected<User, std::string> SqliteRepository::tryGetUser(int64_t telegram_id) {
    SQLite::Statement query(*db_, "SELECT * FROM user WHERE telegram_id = ?");

    query.bind(1, telegram_id);

    try {
        if (query.executeStep()) {
            return User(query.getColumn(0), query.getColumn(1), query.getColumn(2),
                        query.getColumn(3), query.getColumn(4),
                        (int64_t)query.getColumn(5));
        }

        return std::unexpected("No user with telegram_id: " +
                               std::to_string(telegram_id));
    }
    catch (const std::exception& e) {
        return std::unexpected(e.what());
    }
}

std::expected<int64_t, std::string> SqliteRepository::tryAddList(const List& list) {
    SQLite::Statement query(*db_, "INSERT INTO list (name) VALUES (?)");

    query.bind(1, list.name);

    try {
        query.exec();

        int64_t added_list_id = db_->getLastInsertRowid();

        auto add_result = addUsersToList(List(added_list_id, list.name));

        if (!add_result) {
            return std::unexpected(add_result.error());
        }

        return added_list_id;
    }
    catch (const std::exception& e) {
        return std::unexpected(e.what());
    }
}

std::expected<void, std::string> SqliteRepository::tryRemoveList(int64_t list_id) {
    auto get_result = tryGetList(list_id);

    if (!get_result) {
        return std::unexpected(get_result.error());
    }

    List& list = get_result.value();

    SQLite::Statement list_query(*db_, "DELETE FROM list WHERE list_id = ?");
    list_query.bind(1, list.list_id);

    SQLite::Statement user_query(*db_, "DELETE FROM list_user WHERE list_id = ?");
    user_query.bind(1, list.list_id);

    try {
        SQLite::Transaction transaction(*db_);

        list_query.exec();

        user_query.exec();

        transaction.commit();
        return {};
    }
    catch (const std::exception& e) {
        return std::unexpected(e.what());
    }
}

std::expected<List, std::string> SqliteRepository::tryGetList(
    const std::string& list_name) {
    SQLite::Statement query(*db_, "SELECT * FROM list WHERE name = ?");

    query.bind(1, list_name);

    try {
        if (query.executeStep()) {
            return List(query.getColumn(0), query.getColumn(1));
        } else {
            return std::unexpected("No list with name " + list_name);
        }
    }
    catch (const std::exception& e) {
        return std::unexpected(e.what());
    }
}

std::expected<List, std::string> SqliteRepository::tryGetList(int64_t list_id) {
    SQLite::Statement query(*db_, "SELECT * FROM list WHERE list_id = ?");

    query.bind(1, list_id);

    try {
        if (query.executeStep()) {
            return List(query.getColumn(0), query.getColumn(1));
        } else {
            return std::unexpected("No list with id " + std::to_string(list_id));
        }
    }
    catch (const std::exception& e) {
        return std::unexpected(e.what());
    }
}

std::expected<std::vector<List>, std::string> SqliteRepository::tryGetAllLists() {
    SQLite::Statement query(*db_, "SELECT * FROM list");

    std::vector<List> lists;

    try {
        while (query.executeStep()) {
            lists.emplace_back(query.getColumn(0), query.getColumn(1));
        }

        return lists;
    }
    catch (const std::exception& e) {
        return std::unexpected(e.what());
    }
}

std::expected<void, std::string> SqliteRepository::trySwapUsers(int64_t list_id,
                                                                int64_t first_user_id,
                                                                int64_t second_user_id) {
    auto listResult = tryGetList(list_id);
    if (!listResult) {
        return std::unexpected(listResult.error());
    }

    auto firstUserListUserResult = tryGetListUser(list_id, first_user_id);
    if (!firstUserListUserResult) {
        return std::unexpected("First user is not in the list");
    }
    ListUser& firstUserListUser = firstUserListUserResult.value();

    auto secondUserListUserResult = tryGetListUser(list_id, second_user_id);
    if (!secondUserListUserResult) {
        return std::unexpected("Second user is not in the list");
    }
    ListUser& secondUserListUser = secondUserListUserResult.value();

    try {
        SQLite::Transaction transaction(*db_);

        int first_user_order = firstUserListUser.list_user_order;
        int second_user_order = secondUserListUser.list_user_order;

        SQLite::Statement updateFirstQuery(
            *db_,
            "UPDATE list_user SET list_user_order = ? WHERE list_id = ? AND user_id = ?");

        updateFirstQuery.bind(1, second_user_order);
        updateFirstQuery.bind(2, list_id);
        updateFirstQuery.bind(3, first_user_id);
        updateFirstQuery.exec();

        SQLite::Statement updateSecondQuery(
            *db_,
            "UPDATE list_user SET list_user_order = ? WHERE list_id = ? AND user_id = ?");

        updateSecondQuery.bind(1, first_user_order);
        updateSecondQuery.bind(2, list_id);
        updateSecondQuery.bind(3, second_user_id);
        updateSecondQuery.exec();

        transaction.commit();
        return {};
    }
    catch (const std::exception& e) {
        return std::unexpected(e.what());
    }
}

std::expected<void, std::string> SqliteRepository::addUsersToList(const List& list) {
    auto get_result = tryGetAllUsers();

    if (!get_result) {
        return std::unexpected(get_result.error());
    }

    const std::vector<User>& users = get_result.value();

    std::vector<int64_t> order(users.size());

    for (size_t i = 0; i < users.size(); ++i) {
        order[i] = static_cast<int64_t>(i);
    }

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(order.begin(), order.end(), g);

    try {
        SQLite::Transaction transaction(*db_);
        SQLite::Statement query(*db_,
                                "INSERT INTO list_user (list_id, user_id, "
                                "list_user_order) VALUES (?, ?, ?)");

        for (size_t i = 0; i < users.size(); ++i) {
            const User& user = users[i];
            query.bind(1, list.list_id);
            query.bind(2, user.user_id);
            query.bind(3, order[i]);
            query.exec();
            query.reset();
        }

        transaction.commit();
    }
    catch (const std::exception& e) {
        return std::unexpected(e.what());
    }

    return {};
}

std::expected<std::vector<ListUser>, std::string> SqliteRepository::tryGetListUsers(
    int64_t list_id) {
    auto get_result = tryGetList(list_id);

    if (!get_result) {
        return std::unexpected(get_result.error());
    }

    List& list = get_result.value();

    SQLite::Statement list_user_query(*db_, "SELECT * FROM list_user WHERE list_id = ?");
    list_user_query.bind(1, list.list_id);

    std::vector<ListUser> list_users;

    try {
        while (list_user_query.executeStep()) {
            list_users.emplace_back(
                list_user_query.getColumn(0), list_user_query.getColumn(1),
                list_user_query.getColumn(2), list_user_query.getColumn(3));
        }

        return list_users;
    }
    catch (const std::exception& e) {
        return std::unexpected(e.what());
    }
}

std::expected<std::vector<User>, std::string> SqliteRepository::tryGetUsers(
    int64_t list_id) {
    auto get_result = tryGetList(list_id);

    if (!get_result) {
        return std::unexpected(get_result.error());
    }

    List& list = get_result.value();

    SQLite::Statement list_user_query(*db_, "SELECT * FROM list_user WHERE list_id = ?");
    list_user_query.bind(1, list.list_id);

    std::vector<ListUser> list_users;
    std::vector<User> users;

    try {
        while (list_user_query.executeStep()) {
            list_users.emplace_back(
                list_user_query.getColumn(0), list_user_query.getColumn(1),
                list_user_query.getColumn(2), list_user_query.getColumn(3));
        }

        SQLite::Statement get_user_query(*db_, "SELECT * FROM user WHERE user_id = ?");

        for (const ListUser& lu : list_users) {
            get_user_query.bind(1, lu.user_id);
            if (get_user_query.executeStep()) {
                users.emplace_back(
                    get_user_query.getColumn(0), get_user_query.getColumn(1),
                    get_user_query.getColumn(2), get_user_query.getColumn(3),
                    get_user_query.getColumn(4), (int64_t)get_user_query.getColumn(5));
            }
            get_user_query.reset();
        }

        return users;
    }
    catch (const std::exception& e) {
        return std::unexpected(e.what());
    }
}

std::expected<int64_t, std::string> SqliteRepository::tryAddUser(const User& user) {
    if (user.user_id != kEmptyUserId) {
        return std::unexpected("Trying to add user, whos user_id != kEmptyUserId");
    }

    SQLite::Statement query(*db_,
                            "INSERT INTO user (telegram_id, first_name, surname, "
                            "second_name, admin) VALUES (?, ?, ?, ?, ?)");

    query.bind(1, user.telegram_id);
    query.bind(2, user.first_name);
    query.bind(3, user.surname);
    query.bind(4, user.second_name);
    query.bind(5, user.admin ? 1 : 0);

    try {
        int64_t added_user_id = query.exec();
        return added_user_id;
    }
    catch (const std::exception& e) {
        return std::unexpected(e.what());
    }
}

std::expected<ListUser, std::string> SqliteRepository::tryGetListUser(int64_t list_id,
                                                                      int64_t user_id) {
    SQLite::Statement query(*db_,
                            "SELECT * FROM list_user WHERE (list_id, user_id) = (?, ?)");

    query.bind(1, list_id);
    query.bind(2, user_id);

    try {
        if (query.executeStep()) {
            return ListUser(query.getColumn(0), query.getColumn(1), query.getColumn(2),
                            query.getColumn(3));
        }

        return std::unexpected("No list user with user_id: " + std::to_string(user_id) +
                               " in list with id: " + std::to_string(list_id));
    }
    catch (const std::exception& e) {
        return std::unexpected(e.what());
    }
}

std::expected<User, std::string> SqliteRepository::tryGetUserByTelegramId(
    int64_t telegram_id) {
    SQLite::Statement query(*db_, "SELECT * FROM user WHERE telegram_id = ?");

    query.bind(1, telegram_id);

    try {
        if (query.executeStep()) {
            return User(query.getColumn(0), query.getColumn(1), query.getColumn(2),
                        query.getColumn(3), query.getColumn(4),
                        (int64_t)query.getColumn(5));
        }

        return std::unexpected("No user with telegram_id: " +
                               std::to_string(telegram_id));
    }
    catch (const std::exception& e) {
        return std::unexpected(e.what());
    }
}
