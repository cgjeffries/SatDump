#pragma once

#include "core/module.h"
#include "decode_utils.h"
#include <fstream>

namespace inmarsat
{
    namespace stdc
    {
        class STDCParserModule : public ProcessingModule
        {
        protected:
            uint8_t *buffer;

            std::ifstream data_in;
            std::atomic<size_t> filesize;
            std::atomic<size_t> progress;

            std::mutex pkt_history_mtx;
            std::vector<nlohmann::json> pkt_history;
            std::vector<nlohmann::json> pkt_history_msg;
            std::vector<nlohmann::json> pkt_history_egc;

            void write_pkt_file_out(nlohmann::json &msg);

        public:
            STDCParserModule(std::string input_file, std::string output_file_hint, nlohmann::json parameters);
            ~STDCParserModule();
            void process();
            void drawUI(bool window);
            std::vector<ModuleDataType> getInputTypes();
            std::vector<ModuleDataType> getOutputTypes();

        public:
            static std::string getID();
            virtual std::string getIDM() { return getID(); };
            static std::vector<std::string> getParameters();
            static std::shared_ptr<ProcessingModule> getInstance(std::string input_file, std::string output_file_hint, nlohmann::json parameters);
        };
    }
}