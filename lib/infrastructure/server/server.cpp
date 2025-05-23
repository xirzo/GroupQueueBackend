#include "server.h"

Server::Server(std::size_t port, std::shared_ptr<Repository> repository)
    : port_(port), repository_(std::move(repository)) {
    CROW_ROUTE(app_, "/get_all_users").methods(crow::HTTPMethod::Get)([this]() {
        auto get_result = repository_->tryGetAllUsers();

        if (!get_result) {
            return crow::response(400, get_result.error());
        }

        crow::json::wvalue j;

        for (std::size_t i = 0; i < get_result.value().size(); ++i) {
            j[i] = jsonFromUser(get_result.value().at(i));
        }

        return crow::response(200, j);
    });

    CROW_ROUTE(app_, "/get_user_by_telegram_id/<int>")
        .methods(crow::HTTPMethod::Get)([this](int64_t telegram_id) {
            auto get_result = repository_->tryGetUser(telegram_id);

            if (!get_result) {
                return crow::response(400, get_result.error());
            }
            crow::json::wvalue j = jsonFromUser(get_result.value());

            return crow::response(200, j);
        });

    CROW_ROUTE(app_, "/add_list/<string>")
        .methods(crow::HTTPMethod::Post)([this](std::string list_name) {
            auto add_result = repository_->tryAddList(List(list_name));

            if (!add_result) {
                return crow::response(400, add_result.error());
            }

            return crow::response(200, "Successfully created new list!");
        });

    CROW_ROUTE(app_, "/remove_list/<int>")
        .methods(crow::HTTPMethod::Delete)([this](int64_t list_id) {
            auto remove_result = repository_->tryRemoveList(list_id);

            if (!remove_result) {
                return crow::response(400, remove_result.error());
            }

            return crow::response(200, "Successfully removed list!");
        });

    CROW_ROUTE(app_, "/get_list/<string>")
        .methods(crow::HTTPMethod::Get)([this](std::string list_name) {
            auto get_result = repository_->tryGetList(list_name);

            if (!get_result) {
                return crow::response(400, get_result.error());
            }

            crow::json::wvalue j = jsonFromList(get_result.value());

            return crow::response(200, j);
        });

    CROW_ROUTE(app_, "/get_all_lists").methods(crow::HTTPMethod::Get)([this]() {
        auto get_result = repository_->tryGetAllLists();

        if (!get_result) {
            return crow::response(400, get_result.error());
        }

        crow::json::wvalue j;

        for (std::size_t i = 0; i < get_result.value().size(); ++i) {
            j[i] = jsonFromList(get_result.value().at(i));
        }

        return crow::response(200, j);
    });

    CROW_ROUTE(app_, "/swap/<int>/<int>/<int>")
        .methods(crow::HTTPMethod::Post)([this](int64_t list_id,
                                                int64_t sender_telegram_id,
                                                int64_t receiver_telegram_id) {
            auto swap_result = repository_->trySwapUsers(list_id, sender_telegram_id,
                                                         receiver_telegram_id);

            if (!swap_result) {
                return crow::response(400, swap_result.error());
            }

            return crow::response(200, "Successfully swapped users!");
        });

    CROW_ROUTE(app_, "/get_user_by_telegram_id/<int>")
        .methods(crow::HTTPMethod::Post)([this](int64_t telegram_id) {
            auto get_result = repository_->tryGetUserByTelegramId(telegram_id);

            if (!get_result) {
                return crow::response(400, get_result.error());
            }

            return crow::response(200, jsonFromUser(get_result.value()));
        });

    CROW_ROUTE(app_, "/get_list_users/<int>")
        .methods(crow::HTTPMethod::Get)([this](int64_t list_id) {
            auto get_result = repository_->tryGetListUsers(list_id);

            if (!get_result) {
                return crow::response(400, get_result.error());
            }

            crow::json::wvalue j;

            for (std::size_t i = 0; i < get_result.value().size(); ++i) {
                j[i] = jsonFromListUser(get_result.value().at(i));
            }

            return crow::response(200, j);
        });

    CROW_ROUTE(app_, "/get_users/<int>")
        .methods(crow::HTTPMethod::Get)([this](int64_t list_id) {
            auto get_result = repository_->tryGetUsers(list_id);

            if (!get_result) {
                return crow::response(400, get_result.error());
            }

            crow::json::wvalue j;

            for (std::size_t i = 0; i < get_result.value().size(); ++i) {
                j[i] = jsonFromUser(get_result.value().at(i));
            }

            return crow::response(200, j);
        });

    app_.port(port_).multithreaded().run();
}

crow::json::wvalue Server::jsonFromUser(const User& user) {
    crow::json::wvalue j;
    j["user_id"] = user.user_id;
    j["telegram_id"] = user.telegram_id;
    j["first_name"] = user.first_name;
    j["surname"] = user.surname;
    j["second_name"] = user.second_name;
    j["admin"] = user.admin;
    return j;
}

crow::json::wvalue Server::jsonFromListUser(const ListUser& list_user) {
    crow::json::wvalue j;
    j["list_user_id"] = list_user.list_user_id;
    j["list_id"] = list_user.list_id;
    j["user_id"] = list_user.user_id;
    j["list_user_order"] = list_user.list_user_order;
    return j;
}

crow::json::wvalue Server::jsonFromList(const List& list) {
    crow::json::wvalue j;
    j["list_id"] = list.list_id;
    j["name"] = list.name;
    return j;
}
