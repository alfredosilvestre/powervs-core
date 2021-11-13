#include "decklinkcontrol.h"

#include <comutil.h>

#include <log4cxx/logger.h>

using namespace log4cxx;

QList<DeckLinkInput*> DeckLinkControl::deckLinkInputList;
QList<DeckLinkOutput*> DeckLinkControl::deckLinkOutputList;

bool DeckLinkControl::init()
{
    HRESULT	result;

    // Initialize COM
    result = CoInitialize(NULL);
    if (FAILED(result))
    {
        LOG4CXX_ERROR(Logger::getLogger("DeckLinkControl"), "Initialization of COM failed - result = " + QString::number(result).toStdString());
        return false;
    }

    // Create an IDeckLinkIterator object to enumerate all DeckLink cards in the system
    IDeckLinkIterator* deckLinkIterator;
    result = CoCreateInstance(CLSID_CDeckLinkIterator, NULL, CLSCTX_ALL, IID_IDeckLinkIterator, (void**)&deckLinkIterator);
    if(FAILED(result))
    {
        LOG4CXX_ERROR(Logger::getLogger("DeckLinkControl"), "A DeckLink iterator could not be created. The DeckLink drivers may not be installed.");
        return false;
    }

    IDeckLink* deckLinkDevice;
    while(deckLinkIterator->Next(&deckLinkDevice) == S_OK)
    {
        IDeckLinkAttributes *deckLinkAttributes = NULL;
        if(deckLinkDevice->QueryInterface(IID_IDeckLinkAttributes, (void**)&deckLinkAttributes) != S_OK)
        {
            LOG4CXX_ERROR(Logger::getLogger("DeckLinkControl"), "Could not get the IDeckLinkAttributes interface");
            deckLinkAttributes = NULL;

            deckLinkDevice->Release();
            continue;
        }

        LONGLONG sub_devices;
        result = deckLinkAttributes->GetInt(BMDDeckLinkNumberOfSubDevices, &sub_devices);
        if(FAILED(result))
        {
            LOG4CXX_ERROR(Logger::getLogger("DeckLinkControl"), "Cannot get the number of sub devices for Decklink device");
            continue;
        }

        if(sub_devices > 2)
        {
            BOOL supports_duplex;
            result = deckLinkAttributes->GetFlag(BMDDeckLinkSupportsDuplexModeConfiguration, &supports_duplex);
            if(FAILED(result))
            {
                LOG4CXX_ERROR(Logger::getLogger("DeckLinkControl"), "Cannot get the duplex mode flag for Decklink device");
                continue;
            }

            if(supports_duplex)
            {
                IDeckLinkConfiguration *deckLinkConfiguration = NULL;
                if(deckLinkDevice->QueryInterface(IID_IDeckLinkConfiguration, (void**)&deckLinkConfiguration) != S_OK)
                {
                    LOG4CXX_ERROR(Logger::getLogger("DeckLinkControl"), "Could not get the IDeckLinkConfiguration interface");
                    deckLinkConfiguration = NULL;

                    deckLinkDevice->Release();
                    continue;
                }

                result = deckLinkConfiguration->SetInt(bmdDeckLinkConfigDuplexMode, bmdDuplexModeHalf);
                if(FAILED(result))
                {
                    LOG4CXX_ERROR(Logger::getLogger("DeckLinkControl"), "Cannot set the duplex mode for Decklink device");
                }
                else LOG4CXX_INFO(Logger::getLogger("DeckLinkControl"), "Duplex mode configured to HalfDuplex");
            }
        }

        DeckLinkInput* input = new DeckLinkInput(deckLinkDevice);
        if(input->isInitialized())
            deckLinkInputList.append(input);
        else delete input;

        DeckLinkOutput* output = new DeckLinkOutput(deckLinkDevice);
        if(output->isInitialized())
            deckLinkOutputList.append(output);
        else delete output;

        deckLinkDevice->Release();
    }

    return true;
}

void DeckLinkControl::destroy()
{
    qDeleteAll(deckLinkInputList);
    deckLinkInputList.clear();
    qDeleteAll(deckLinkOutputList);
    deckLinkOutputList.clear();

    // Uninitalize COM
    CoUninitialize();
}

const QStringList DeckLinkControl::getInputList()
{
    QStringList inputList;
    for(int i=0; i<deckLinkInputList.size(); i++)
        inputList.append(deckLinkInputList.at(i)->getDeviceName() + " Input (" + QString::number(i+1) + ")");

    return inputList;
}

const QStringList DeckLinkControl::getOutputList()
{
    QStringList outputList;
    for(int i=0; i<deckLinkOutputList.size(); i++)
        outputList.append(deckLinkOutputList.at(i)->getDeviceName() + " Output (" + QString::number(i+1) + ")");

    return outputList;
}

DeckLinkInput* DeckLinkControl::getInput(const int pos)
{
    return deckLinkInputList.at(pos-1);
}

DeckLinkOutput* DeckLinkControl::getOutput(const int pos)
{
    return deckLinkOutputList.at(pos-1);
}
