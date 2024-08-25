#define PREDICTOR_H_SEEN

#include <cstdint>
#include <vector>
#include "op_state.h"
#include "tread.h"

typedef enum {
    STROGNLY_USE_PREDICTOR_1,
    WEAKLY_USE_PREDICTOR_1,
    WEAKLY_USE_PREDICTOR_2,
    STRONGLY_USE_PREDICTOR_2,
} CHOICE_ENUM;

class PREDICTOR {
public:
    bool get_prediction(const branch_record_c* br, const op_state_c* os);
    void update_predictor(const branch_record_c* br, const op_state_c* os, bool taken);

private:
    void update_path_history(bool taken);
    void update_choice_prediction_table(uint32_t global_prediction_index, bool taken);
    void update_global_predictor(const branch_record_c* br, bool taken);
    void update_global_prediction_table(uint32_t global_prediction_index, bool taken);
    void update_local_prediction_table(uint32_t LP_bits, bool taken);
    void update_local_predictor(const branch_record_c* br, bool taken);
    void update_local_history_table(uint32_t PC_bits, bool taken);

    CHOICE_ENUM choice_prediction;
};
