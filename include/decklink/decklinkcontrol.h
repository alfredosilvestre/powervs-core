#ifndef DECKLINKCONTROL_H
#define DECKLINKCONTROL_H

#include "DeckLinkAPI_h.h"
#include "decklinkinput.h"
#include "decklinkoutput.h"

#include <QStringList>

class DeckLinkControl
{
    public:
        static bool init();
        static void destroy();

    public:
        static const QStringList getInputList();
        static const QStringList getOutputList();
        static DeckLinkInput* getInput(const int pos);
        static DeckLinkOutput* getOutput(const int pos);

    private:
        static QList<DeckLinkInput*> deckLinkInputList;
        static QList<DeckLinkOutput*> deckLinkOutputList;
};

#endif // DECKLINKCONTROL_H
