#include "pipeline_selector.h"

#include "core/config.h"
#include "imgui/imgui.h"
#include "common/utils.h"
#include "imgui/imgui_stdlib.h"
#include "common/detect_header.h"

namespace satdump
{
    PipelineUISelector::PipelineUISelector(bool live_mode) : live_mode(live_mode)
    {
        nlohmann::ordered_json params = satdump::config::main_cfg["user_interface"]["default_offline_parameters"];

        for (nlohmann::detail::iteration_proxy_value<nlohmann::detail::iter_impl<nlohmann::ordered_json>> cfg : params.items())
            if (!cfg.value().contains("no_live") || !live_mode)
                parameters_ui.push_back({ cfg.key(), satdump::params::EditableParameter(nlohmann::json(cfg.value())) });

        if (config::main_cfg.contains("user"))
        {
            if (config::main_cfg["user"].contains("favourite_pipelines"))
            {
                try
                {
                    for (std::string pipeline_s : config::main_cfg["user"]["favourite_pipelines"].get<std::vector<std::string>>())
                    {
                        for (int i = 0; i < (int)pipelines.size(); i++)
                            if (pipelines[i].name == pipeline_s)
                                favourite.push_back(i);
                    }
                }
                catch (std::exception &)
                {
                }
            }
        }
    }

    bool PipelineUISelector::contains(std::vector<int> tm, int n)
    {
        for (unsigned int i = 0; i < tm.size(); i++)
        {
            if (tm[i] == n)
                return true;
        }
        return false;
    }

    void PipelineUISelector::getParamsFromInput()
    {
        std::string file_path = inputfileselect.getPath();

        if (std::filesystem::exists(file_path) && !std::filesystem::is_directory(file_path))
        {
            nlohmann::json hdr;
            try_get_params_from_input_file(hdr, file_path);
            for (auto &v : hdr.items())
                try_set_param(v.key(), v.value());
        }
    }

    void PipelineUISelector::try_set_param(std::string name, nlohmann::json v)
    {
        for (std::pair<std::string, satdump::params::EditableParameter> &p : parameters_ui)
            if (p.first == name)
                p.second.setValue(v);

        for (std::pair<std::string, satdump::params::EditableParameter> &p : parameters_ui_pipeline)
            if (p.first == name)
                p.second.setValue(v);
    }

    void PipelineUISelector::updateSelectedPipeline()
    {
        parameters_ui_pipeline.clear();
        for (nlohmann::detail::iteration_proxy_value<nlohmann::detail::iter_impl<nlohmann::json>> cfg : pipelines[pipeline_id].editable_parameters.items())
        {
            auto it = std::find_if(parameters_ui.begin(),
                parameters_ui.end(),
                [&cfg](const std::pair<std::string, satdump::params::EditableParameter> &e)
                {
                    return e.first == cfg.key();
                });

            if (live_mode)
            {
                if (it == parameters_ui.end() && cfg.value().contains("type"))
                    parameters_ui_pipeline.push_back({ cfg.key(), satdump::params::EditableParameter(cfg.value()) });
                else if (!cfg.value().contains("type") && it != parameters_ui.end())
                    it->second.setValue(cfg.value()["value"]);
            }
            else
            {
                if (it == parameters_ui.end())
                    parameters_ui_pipeline.push_back({ cfg.key(), satdump::params::EditableParameter(cfg.value()) });
                else
                    it->second.setValue(cfg.value()["value"]);
            }
        }

        if (!live_mode)
        {
            pipeline_levels_str = "";
            if (pipeline_id != -1)
                for (int i = 0; i < (int)pipelines[pipeline_id].steps.size() - 1; i++)
                    pipeline_levels_str += pipelines[pipeline_id].steps[i].level_name + '\0';

            if (pipelines_levels_select_id == -1)
                pipelines_levels_select_id = 0;

            if (pipelines[pipeline_id].editable_parameters.size() != 0)
                getParamsFromInput();
        }
    }

    void PipelineUISelector::renderSelectionBox(double width)
    {
        pipeline_mtx.lock();
        if (width != -1)
            ImGui::SetNextItemWidth(width);
        ImGui::InputTextWithHint("##pipelinesearchbox", u8"\uf422   Search pipelines", &pipeline_search_in);
        if (width != -1)
            ImGui::SetNextItemWidth(width);

        if (config::main_cfg.contains("user"))
        {
            if (config::main_cfg["user"].contains("favourite_pipelines"))
            {
                if (config::main_cfg["user"]["favourite_pipelines"].size() != favourite.size())
                {
                    favourite.clear();
                    for (std::string pipeline_s : config::main_cfg["user"]["favourite_pipelines"].get<std::vector<std::string>>())
                    {
                        for (int i = 0; i < (int)pipelines.size(); i++)
                            if (pipelines[i].name == pipeline_s)
                                favourite.push_back(i);
                    }
                }
            }
        }

        if (ImGui::BeginListBox("##pipelineslistbox"))
        {
            ImVec4 color = { 0.73, 0.6, 0.15, 1.0 };
            bool show = !live_mode;
            if (live_mode)
            {
                for (int p : favourite)
                    if (pipelines[p].live)
                    {
                        show = true;
                        break;
                    }
            }

            if (!favourite.empty() && show)
            {
                if (ImGui::CollapsingHeader("Favourites", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::Spacing();
                    for (int k = 0; k < (int)favourite.size(); k++)
                    {
                        int n = favourite[k];
                        bool show = true;
                        if (pipeline_search_in.size() != 0)
                            show = isStringPresent(pipelines[n].readable_name, pipeline_search_in);

                        if (show && (!live_mode || pipelines[n].live))
                        {
                            bool is_selected = (pipeline_id == n);
                            ImGui::Selectable((pipelines[n].readable_name + "##fav").c_str(), &is_selected);
                            if (ImGui::IsItemHovered())
                            {
                                int pos = ImGui::GetItemRectSize().x - 25;
                                ImGui::SameLine(pos);
                                ImGui::TextColored({ 0, 0, 0, 0 }, "%s", text.c_str());

                                if (is_selected != (pipeline_id == n) && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlapped))
                                {
                                    favourite.erase(favourite.begin() + k);
                                    config::main_cfg["user"]["favourite_pipelines"].erase(k);
                                    continue;
                                }

                                ImGui::SameLine(pos);
                                text = u8"\uf005";
                                ImGui::TextColored(color, "%s", text.c_str());
                                text = u8"\uf006";
                                if (is_selected != (pipeline_id == n))
                                {
                                    pipeline_id = n;
                                    updateSelectedPipeline();
                                }
                            }
                            if (is_selected)
                            {
                                // pipeline_id = n;
                                // updateSelectedPipeline();
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::Spacing();
            }
            for (int n = 0; n < (int)pipelines.size(); n++)
            {
                bool show = true;
                if (pipeline_search_in.size() != 0)
                    show = isStringPresent(pipelines[n].readable_name, pipeline_search_in);

                if (show && (!live_mode || pipelines[n].live))
                {
                    bool is_selected = (pipeline_id == n);
                    bool isfav = contains(favourite, n);
                    ImGui::Selectable(pipelines[n].readable_name.c_str(), &is_selected);
                    if (ImGui::IsItemHovered() || isfav)
                    {
                        int pos = ImGui::GetItemRectSize().x - 25;
                        ImGui::SameLine(pos);
                        ImGui::TextColored({ 0, 0, 0, 0 }, "%s", text.c_str());

                        if (is_selected != (pipeline_id == n) && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlapped))
                        {
                            if (isfav)
                            {
                                for (int i = 0; i < (int)favourite.size(); i++)
                                {
                                    if (favourite[i] == n)
                                    {
                                        favourite.erase(favourite.begin() + i);
                                        config::main_cfg["user"]["favourite_pipelines"].erase(i);
                                        isfav = false;
                                        break;
                                    }
                                }
                                continue;
                            }
                            else
                            {
                                favourite.push_back(n);
                                config::main_cfg["user"]["favourite_pipelines"].push_back(pipelines[n].name);
                                isfav = true;
                                is_selected = !is_selected;
                            }
                        }

                        ImGui::SameLine(pos);
                        if (isfav)
                            text = u8"\uf005";
                        ImGui::TextColored(color, "%s", text.c_str());
                        text = u8"\uf006";
                        if (is_selected != (pipeline_id == n))
                        {
                            pipeline_id = n;
                            updateSelectedPipeline();
                        }
                    }
                    if (is_selected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
            }
            ImGui::EndListBox();
        }
        pipeline_mtx.unlock();
    }

    void PipelineUISelector::drawMainparams()
    {
        pipeline_mtx.lock();
        if (ImGui::BeginTable("##pipelinesmainoptions", 2))
        {
            ImGui::TableSetupColumn("##pipelinesmaincolumn1", ImGuiTableColumnFlags_WidthFixed, 100 * ui_scale);
            ImGui::TableSetupColumn("##pipelinesmaincolumn2", ImGuiTableColumnFlags_WidthStretch, 100 * ui_scale);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Input File");
            ImGui::TableSetColumnIndex(1);
            if (inputfileselect.draw())
                getParamsFromInput();

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Output Directory");
            ImGui::TableSetColumnIndex(1);
            outputdirselect.draw();

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Input Level");
            ImGui::TableSetColumnIndex(1);
            ImGui::Combo("##pipelinelevel", &pipelines_levels_select_id, pipeline_levels_str.c_str());
            ImGui::EndTable();
        }

        pipeline_mtx.unlock();
    }

    void PipelineUISelector::drawMainparamsLive()
    {
        ImGui::Text("Output Directory :");
        outputdirselect.draw();
        ImGui::Spacing();
    }

    void PipelineUISelector::renderParamTable()
    {
        if (ImGui::BeginTable("##pipelineoptions", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            for (std::pair<std::string, satdump::params::EditableParameter> &p : parameters_ui)
                p.second.draw();
            for (std::pair<std::string, satdump::params::EditableParameter> &p : parameters_ui_pipeline)
                p.second.draw();
            ImGui::EndTable();
        }
    }

    nlohmann::json PipelineUISelector::getParameters()
    {
        nlohmann::json params2;
        for (std::pair<std::string, satdump::params::EditableParameter> &p : parameters_ui)
            params2[p.first] = p.second.getValue();
        for (std::pair<std::string, satdump::params::EditableParameter> &p : parameters_ui_pipeline)
            params2[p.first] = p.second.getValue();
        return params2;
    }

    void PipelineUISelector::setParameters(nlohmann::json params)
    {
        pipeline_mtx.lock();
        for (auto &el : params.items())
            try_set_param(el.key(), el.value());
        pipeline_mtx.unlock();
    }

    void PipelineUISelector::select_pipeline(std::string id)
    {
        pipeline_mtx.lock();
        for (int n = 0; n < (int)pipelines.size(); n++)
        {
            if (id == pipelines[n].name)
                pipeline_id = n;
        }

        updateSelectedPipeline();
        pipeline_mtx.unlock();
    }

    std::string PipelineUISelector::get_name(int index)
    {
        return pipelines[index].readable_name;
    }
}