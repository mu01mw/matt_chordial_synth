/*
  ==============================================================================

    ChordialModMatrix.h
    Created: 14 Sep 2018 9:46:27am
    Author:  matth

  ==============================================================================
*/

#pragma once
namespace chordial
{
namespace synth
{

class ChordialModMatrixCore
{
    struct MatrixRow
    {
        MatrixRow(const std::string& source, const std::string& destination, bool enabled) : source(source), destination(destination), enabled(enabled) {}
        const std::string source, destination;
        bool enabled;
    };

public:
    void addRow(const std::string& source, const std::string& destination, bool enabled)
    {
        rows.emplace_back(source, destination, enabled);
    }

    const std::vector<MatrixRow>& getRows() { return rows; }

private:
    std::vector<MatrixRow> rows;
};

template <typename SampleType>
class ChordialModMatrix
{
public:
    struct ModMatrixSource
    {
        const std::string name;
        SampleType* const valPtr;
    };
    struct ModMatrixDestination
    {
        const std::string name;
        SampleType* const valPtr;
    };

    void setCore(std::shared_ptr<ChordialModMatrixCore> matrixCore)
    {
        core = matrixCore;
    }
    void addModSource(ModMatrixSource source)
    {
        modSources[source.name] = source.valPtr;
    }

    void addModDestination(ModMatrixDestination destination)
    {
        modDestinations[destination.name] = destination.valPtr;
    }

    void process()
    {
        
        if (core == nullptr)
            return;

        clearDestinations();
        for (const auto& row : core->getRows())
            if (row.enabled)
                *modDestinations[row.destination] += *modSources[row.source];
    }

private:

    void clearDestinations()
    {
        for (const auto& row : core->getRows())
        {
            *modDestinations[row.destination] = static_cast<SampleType>(0.0);
        }
    }

    std::shared_ptr<ChordialModMatrixCore> core{ nullptr };
    std::unordered_map<std::string, SampleType*> modSources;
    std::unordered_map<std::string, SampleType*> modDestinations;
};

}
}