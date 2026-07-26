#ifndef PLAYERBOTS_ULDMULTIPLIERS_H
#define PLAYERBOTS_ULDMULTIPLIERS_H

#include "Multiplier.h"

// Algalon the Observer
class AlgalonMultiplier : public Multiplier
{
public:
    AlgalonMultiplier(PlayerbotAI* ai) : Multiplier(ai, "algalon") {}
    virtual float GetValue(Action* action);
};

#endif
