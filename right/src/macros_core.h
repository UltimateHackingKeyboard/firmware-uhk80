#ifndef __MACROS_CORE_H__
#define __MACROS_CORE_H__

// Includes:

// Macros:

// Typedefs:

    typedef enum {
        MacroResult_InProgressFlag = 1,
        MacroResult_ActionFinishedFlag = 2,
        MacroResult_DoneFlag = 4,
        MacroResult_YieldFlag = 8,
        MacroResult_BlockingFlag = 16,
        MacroResult_Blocking = MacroResult_InProgressFlag | MacroResult_BlockingFlag,
        MacroResult_Waiting = MacroResult_InProgressFlag | MacroResult_YieldFlag,
        MacroResult_Sleeping = MacroResult_InProgressFlag | MacroResult_YieldFlag,
        MacroResult_Finished = MacroResult_ActionFinishedFlag,
        MacroResult_JumpedForward = MacroResult_DoneFlag,
        MacroResult_JumpedBackward = MacroResult_DoneFlag | MacroResult_YieldFlag,
    } macro_result_t;

// Variables:

// Functions:

#endif
