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
                .content = (char*)"Counting bot initialized. Type `s!channel` to lock me to a channel!"
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
uint64_t active_channel_id = 0; // The global channel ID lock

void onMessage(struct discord* client, const struct discord_message* event) {
        if (event->author->bot) return;

        std::string content = event->content;

        // 1. Listen for the setup command
        if (content == "s!channel") {
                active_channel_id = event->channel_id;
                
                struct discord_create_message params = {
                        .content = (char*)"✅ Channel locked! I will only count in this channel now. The next number is 1."
                };
                discord_create_message(client, event->channel_id, &params, nullptr);
                
                // Reset the game state just in case it was moved mid-game
                counter = 0;
                lastUser = 0;
                return;
        }

        // 2. Enforce the channel lock (ignore messages from other channels, or if it hasn't been set yet)
        if (event->channel_id != active_channel_id || active_channel_id == 0) {
                return;
        }

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

        if (result.isUndefined() || result.isAborted()) {
                message("Failed to calculate answer.");
                return;
        }

        // At this point it has an answer

        if(!result.isReal(){
                // Print exactly what the engine evaluated for non-real numbers
                std::string debugStr = result.print();
                message("Not real. The engine evaluated this to: " + debugStr);
                return;
        }

        // At this point the answer is real

        if (!result.representsInteger() {
            // Round the answer and check if its an int
            std::string debugStr = result.print();
            
            if(!(result.round(5, 0).representsInteger()))
                message("Not an Integer. The engine evaluated this to: " + result.round(5, 0) + " in 5 decimal places");
                return;
        }

        if (!result.number().isNegative()) {
                uint64_t answer;

                try {
                        std::string stringAnswer = result.print();
                        answer = std::stoull(stringAnswer);
                        std::cout<< "Answer: " << stringAnswer << "\n";
                } catch (...) {
                        message("Failed to convert to string.");
                        return;
                }

                // Rule Enforcement: The Double-Count Punisher
                if (event->author->id == lastUser) {
                        std::stringstream ss;
                        ss << "❌ You can't count twice in a row! You ruined the streak at " << counter << ". The next number is 1.\n";
                        counter = 0;
                        lastUser = 0;
                        message(ss.str());
                        return;
                }

                // Sequence Enforcement: The Out-of-Order Punisher
                if (answer != counter + 1) {
                        std::stringstream ss;
                        ss<< "Failed at " << (counter + 1) << " !! The next number is 1.\n";
                        counter = 0;
                        lastUser = 0;
                        message(ss.str());
                        return;
                }

                lastUser = event->author->id;
                // React with the ballot box with check emoji string
                discord_create_reaction(client, event->channel_id, event->id, 0, "☑️", nullptr);
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
        const char* botToken = "";

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