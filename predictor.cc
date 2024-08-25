//************************************* TEAM 19 ***************************************************//
//************************************ TEAM MEMBERS :**********************************************//
//*********************************1. JOHNATHAN THOMAS ********************************************//
//*********************************2. SAIKIRAN RANGA **********************************************//

#include "predictor.h"
#include <iostream>

// Tuned parameters
const uint32_t NEW_GLOBAL_HISTORY_BITS = 14; // Increased global history length
const uint32_t NEW_GLOBAL_HISTORY_MASK = (1 << NEW_GLOBAL_HISTORY_BITS) - 1;

// Incorporating path information
const uint32_t PATH_HISTORY_BITS = 4;
const uint32_t PATH_HISTORY_MASK = (1 << PATH_HISTORY_BITS) - 1;

//***************************GLOBAL VARIABLES**********************************************//
std::vector<uint32_t> local_ht(1024, 0);
std::vector<uint32_t> local_pt(1024, 0);
std::vector<uint32_t> global_prediction_table(4096, 0);
std::vector<CHOICE_ENUM> choice_prediction_table(4096, CHOICE_ENUM::WEAKLY_USE_PREDICTOR_1);

uint32_t PC_mask = 0xFFC;
uint32_t LH_mask = 0x3FF;
uint32_t LP_mask = 0x7;
uint32_t GLOBAL_HISTORY_MASK = 0xFFF;
uint32_t GP_mask = 0x7;

uint32_t PC_bits;
uint32_t LP_index;
uint32_t LP_bits;

uint32_t local_prediction = 0;
uint32_t global_prediction = 0;
uint32_t prediction = 0;
uint32_t global_prediction_value = 0;

uint32_t choice_index=0;
uint32_t global_prediction_index;

CHOICE_ENUM choice_prediction;
uint32_t path_history;

//***************************************************************************************************//

bool PREDICTOR::get_prediction(const branch_record_c* br, const op_state_c* os) {

    if (!br->is_conditional) {
        return local_prediction;
    } else {
        //*****************************local*********************************
        PC_bits = (br->instruction_addr & PC_mask) >> 2;
        LP_index = local_ht[PC_bits] & LH_mask;
        LP_bits = LP_mask & local_pt[LP_index];
        local_prediction = (LP_bits < 4) ? false : true;
        //******************************local*********************************

        //****************************global**************************************
        global_prediction_index = (GLOBAL_HISTORY_MASK & path_history);
        global_prediction_value = (global_prediction_table[global_prediction_index] & 0x3);
        global_prediction = global_prediction_value >> 1;
        //*****************************global***************************************

        //**************************CHOICE_PREDICTION ******************************

        choice_prediction = choice_prediction_table[global_prediction_index];

        if (choice_prediction < WEAKLY_USE_PREDICTOR_2) {
            prediction = global_prediction;
        } else {
            prediction = local_prediction;
        }
        return prediction;
    }
}

//***************************************************************************************

void PREDICTOR::update_predictor(const branch_record_c* br, const op_state_c* os, bool taken) {


    if (!br->is_conditional) {
        return;
    }
    else
    {
        update_local_predictor(br, taken);
        update_global_predictor(br, taken);
    }
}

void PREDICTOR::update_global_predictor(const branch_record_c* br, bool taken) {
    global_prediction_index = (GLOBAL_HISTORY_MASK & path_history);
    update_global_prediction_table(global_prediction_index, taken);
    update_choice_prediction_table(global_prediction_index, taken);
    update_path_history(taken);
}

void PREDICTOR::update_local_predictor(const branch_record_c* br, bool taken) {
    PC_bits = (br->instruction_addr & PC_mask) >> 2;
    LP_index = local_ht[PC_bits] & LH_mask;
    update_local_prediction_table(LP_index, taken);
    update_local_history_table(PC_bits, taken);
}

void PREDICTOR::update_local_history_table(uint32_t PC_bits, bool taken) {
    if (taken)
        local_ht[PC_bits] = (local_ht[PC_bits] << 1) | 0x1;
    else
        local_ht[PC_bits] = (local_ht[PC_bits] << 1);
}

void PREDICTOR::update_local_prediction_table(uint32_t LP_index, bool taken) {
    if (taken) {
        if (local_pt[LP_index] < 7)
            local_pt[LP_index]++;
    } else {
        if (local_pt[LP_index] > 0)
            local_pt[LP_index]--;
    }

}

void PREDICTOR::update_choice_prediction_table(uint32_t global_prediction_index, bool taken) {
    bool Use_Predictor_1 = global_prediction;
    bool Use_Predictor_2 = local_prediction;

    CHOICE_ENUM& current_choice = choice_prediction_table[global_prediction_index];

    // Using a simple counter approach to introduce hysteresis
    static std::vector<int> hysteresis_counters(choice_prediction_table.size(), 0);
    const int HYSTERESIS_THRESHOLD = 2; // Threshold for changing predictor preference

    auto updateHysteresis = [&](bool increase) {
        if (increase) {
            if (hysteresis_counters[global_prediction_index] < HYSTERESIS_THRESHOLD) {
                hysteresis_counters[global_prediction_index]++;
            }
        } else {
            if (hysteresis_counters[global_prediction_index] > -HYSTERESIS_THRESHOLD) {
                hysteresis_counters[global_prediction_index]--;
            }
        }
    };

    switch (current_choice) {
        case STROGNLY_USE_PREDICTOR_1:
            if (Use_Predictor_1 != taken && Use_Predictor_2 == taken) {
                updateHysteresis(false); // Decrease confidence in Predictor 1
                if (hysteresis_counters[global_prediction_index] <= -HYSTERESIS_THRESHOLD) {
                    current_choice = WEAKLY_USE_PREDICTOR_1;
                    hysteresis_counters[global_prediction_index] = 0; // Reset hysteresis counter
                }
            }
            break;

        case WEAKLY_USE_PREDICTOR_1:
            if (Use_Predictor_1 == taken && Use_Predictor_2 != taken) {
                updateHysteresis(true); // Increase confidence in Predictor 1
                if (hysteresis_counters[global_prediction_index] >= HYSTERESIS_THRESHOLD) {
                    current_choice = STROGNLY_USE_PREDICTOR_1;
                    hysteresis_counters[global_prediction_index] = 0; // Reset hysteresis counter
                }
            } else if (Use_Predictor_1 != taken && Use_Predictor_2 == taken) {
                updateHysteresis(false); // Decrease confidence in Predictor 1
                if (hysteresis_counters[global_prediction_index] <= -HYSTERESIS_THRESHOLD) {
                    current_choice = WEAKLY_USE_PREDICTOR_2;
                    hysteresis_counters[global_prediction_index] = 0; // Reset hysteresis counter
                }
            }
            break;

        case WEAKLY_USE_PREDICTOR_2:
            if (Use_Predictor_1 == taken && Use_Predictor_2 != taken) {
                updateHysteresis(true); // Increase confidence in Predictor 2
                if (hysteresis_counters[global_prediction_index] >= HYSTERESIS_THRESHOLD) {
                    current_choice = WEAKLY_USE_PREDICTOR_1;
                    hysteresis_counters[global_prediction_index] = 0; // Reset hysteresis counter
                }
            } else if (Use_Predictor_1 != taken && Use_Predictor_2 == taken) {
                updateHysteresis(false); // Decrease confidence in Predictor 2
                if (hysteresis_counters[global_prediction_index] <= -HYSTERESIS_THRESHOLD) {
                    current_choice = STRONGLY_USE_PREDICTOR_2;
                    hysteresis_counters[global_prediction_index] = 0; // Reset hysteresis counter
                }
            }
            break;

        case STRONGLY_USE_PREDICTOR_2:
            if (Use_Predictor_1 == taken && Use_Predictor_2 != taken) {
                updateHysteresis(true); // Increase confidence in Predictor 2
                if (hysteresis_counters[global_prediction_index] >= HYSTERESIS_THRESHOLD) {
                    current_choice = WEAKLY_USE_PREDICTOR_2;
                    hysteresis_counters[global_prediction_index] = 0; // Reset hysteresis counter
                }
            }
            break;

        default:
            // Fallback case, should not be reached
            current_choice = taken ? STROGNLY_USE_PREDICTOR_1 : WEAKLY_USE_PREDICTOR_2;
            break;
    }
}




void PREDICTOR::update_global_prediction_table(uint32_t global_prediction_index, bool taken) {
    if (taken)
    {
        if (global_prediction_value < 3)
            global_prediction_table[global_prediction_index]++;
    }
    else
    {
        if (global_prediction_value > 0)
            global_prediction_table[global_prediction_index]--;
    }

    // Ensure values stay within the valid range (0 to 3)
    global_prediction_table[global_prediction_index] = std::min(std::max(static_cast<int>(global_prediction_table[global_prediction_index]), 0), 3);
}


void PREDICTOR::update_path_history(bool taken) {
    uint32_t path_bits = (path_history >> (NEW_GLOBAL_HISTORY_BITS - PATH_HISTORY_BITS)) & PATH_HISTORY_MASK;
    if (taken) {
        path_history = ((path_history << 1) | 0x1) & NEW_GLOBAL_HISTORY_MASK;
    } else {
        path_history = (path_history << 1) & NEW_GLOBAL_HISTORY_MASK;
    }
    // Incorporate path bits based on the branch address to add context
    path_history = (path_history ^ path_bits) & NEW_GLOBAL_HISTORY_MASK;
}
