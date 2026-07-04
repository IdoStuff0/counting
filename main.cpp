#include "discord.h"
#include <libqalculate/includes.h>
#include <libqalculate/qalculate.h>

#include <string>
#include <sstream>
#include <cstdint>
#include <iostream>
#include <thread>

void onReady(struct discord* client, const struct discord_ready* event) {
        printf("Connected\n");
}

void onGuildCreate(struct discord* client, const struct discord_guild* guild) {
        struct discord_create_message params = {
                .content = (char*)"Counting bot initialized. Starting number is 1."
        };

        discord_create_message(
                client,
                guild->system_channel_id,
                &params,
                nullptr
        );
}

Calculator calc;
MathStructure result;
uint64_t counter = 0;
uint64_t lastUser{0};

void onMessage(struct discord* client, const struct discord_message* event) {
        if (event->author->bot) return;
        std::cout<< event->author->username << ": " << event->content << "\n";

        auto message = [&](const std::string& text) {
                struct discord_create_message params = {
                        .content = (char*)text.c_str()
                };
                discord_create_message(
                        client,
                        event->channel_id,
                        &params,
                        nullptr
                );

                std::cout<< "Bot: " << text << "\n";
        };

        calc.calculate(&result, event->content, 2000, default_user_evaluation_options);

        if (result.containsUnknowns()){
                return;
        }

        if (event->author->id == lastUser) {
            //    message("You can't count twice in a row.");
              //  return;
        }

        if (result.isUndefined() || result.isAborted()) {
                message("Failed to calculate answer.");
                return;
        }

        if(!result.representsInteger()){
		//test
                message("Not an integer");

        }

        if (result.representsInteger() && !result.number().isNegative()) {
                uint64_t answer;

                try {
                        std::string stringAnswer = result.print();
                        answer = std::stoull(stringAnswer);
                        std::cout<< "Answer: " << stringAnswer << "\n";
                } catch (...) {
                        message("Failed to convert to string.");
                        return;
                }

                if (answer != counter + 1) {
                        std::stringstream ss;
                        ss<< "Failed at " << (counter + 1) << " !! The next number is 1.\n";
                        counter = 0;
                        lastUser = 0;
                        message(ss.str());
                        return;
                }


                lastUser = event->author->id;
                discord_create_reaction(client, event->channel_id, event->id, 0, "✅", nullptr);
                counter++;
                return;
        }

        message("Missed all conditions");
}

void resetLog() {
        fflush(stdout);
        fflush(stderr);
        freopen("log.txt", "w", stdout);
        freopen("log.txt", "w", stderr);
}

void initLog() {
        while (true) {
                resetLog();
                std::this_thread::sleep_for(std::chrono::minutes(10));
        }
}

int main() {
        const char* botToken = getenv("COUNTING_BOT_TOKEN");

        if (!botToken) {
                printf("Enter token in .env\n");
                return -1;
        }

        std::thread(initLog).detach();

        calc.loadExchangeRates();
        calc.loadGlobalDefinitions();
        calc.loadLocalDefinitions();

        struct discord* client = discord_init(botToken);
        discord_add_intents(client, DISCORD_GATEWAY_MESSAGE_CONTENT);
        discord_set_on_ready(client, &onReady);
        discord_set_on_message_create(client, &onMessage);
        discord_run(client);
}
