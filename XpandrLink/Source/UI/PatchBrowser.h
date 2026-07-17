/*
  PatchBrowser.h
  Version: 26.0 (Fixed Directory Name)
*/
#pragma once
#include <JuceHeader.h>
#include "ThemeData.h"

class PatchBrowser : public juce::Component, public juce::FileBrowserListener
{
public:
    PatchBrowser()
    {
        // 1. Setup Directory - Reverted to OberheimPatches per user request
        rootDirectory = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                            .getChildFile("OberheimPatches");
        
        if (!rootDirectory.exists()) rootDirectory.createDirectory();

        // 2. Thread
        thread.startThread(); 

        // 3. List Setup
        fileFilter.reset(new juce::WildcardFileFilter("*.syx", "*", "Sysex Patches"));
        dirContents.reset(new juce::DirectoryContentsList(fileFilter.get(), thread));
        dirContents->setDirectory(rootDirectory, true, true);
        
        fileList.reset(new juce::FileListComponent(*dirContents));
        fileList->addListener(this);

        // Colors
        fileList->setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff111111));
        fileList->setColour(juce::ListBox::textColourId, juce::Colours::white);
        
        addAndMakeVisible(fileList.get());
    }

    ~PatchBrowser() override {
        fileList->removeListener(this);
        thread.stopThread(2000);
    }

    std::function<void(juce::File)> onPatchSelected;
    std::function<void()>          onRootChanged;   // fired when directory changes

    void refresh() { if (dirContents) dirContents->refresh(); }
    juce::File getRootDirectory() const { return rootDirectory; }

    void setRootDirectory(const juce::File& dir)
    {
        rootDirectory = dir;
        if (!rootDirectory.exists()) rootDirectory.createDirectory();
        if (dirContents) dirContents->setDirectory(rootDirectory, true, true);
        repaint();
        if (onRootChanged) onRootChanged();
    }

    bool canNavigateUp() const { return rootDirectory.getParentDirectory() != rootDirectory; }

    void navigateUp()
    {
        if (canNavigateUp())
            setRootDirectory(rootDirectory.getParentDirectory());
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff181818));
        g.setColour(juce::Colour(0xff004466)); 
        g.fillRect(0, 0, getWidth(), 24);
        g.setColour(juce::Colours::white); 
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        g.drawText("PATCHES", 0, 0, getWidth(), 24, juce::Justification::centred);
        g.setColour(juce::Colour(0xff0099cc)); 
        g.drawRect(getLocalBounds(), 1);
        
        if (dirContents && dirContents->getNumFiles() == 0) {
            g.setColour(juce::Colours::grey);
            g.setFont(12.0f);
            g.drawText("No .syx files found in:\n" + rootDirectory.getFullPathName(),
                       getLocalBounds().reduced(10).withTrimmedTop(24),
                       juce::Justification::centred);
        }
    }

    void resized() override { 
        if (fileList) fileList->setBounds(getLocalBounds().reduced(1).withTrimmedTop(24)); 
    }

    void selectionChanged() override
    {
        // Fires on keyboard arrow navigation — deliver to listeners same as a click.
        if (!fileList || !dirContents) return;
        int row = fileList->getSelectedRow();
        if (row < 0 || row >= dirContents->getNumFiles()) return;
        juce::File f = dirContents->getFile(row);
        if (f.existsAsFile() && onPatchSelected) onPatchSelected(f);
    }
    void fileClicked(const juce::File& file, const juce::MouseEvent&) override
    {
        if (file.existsAsFile() && onPatchSelected) onPatchSelected(file);
    }
    void fileDoubleClicked(const juce::File& file) override
    {
        if (file.isDirectory())
            setRootDirectory(file);   // navigate into subfolder
        else if (file.existsAsFile() && onPatchSelected)
            onPatchSelected(file);
    }
    void browserRootChanged(const juce::File&) override {}

private:
    juce::File rootDirectory;
    juce::TimeSliceThread thread { "PatchBrowserThread" };
    std::unique_ptr<juce::WildcardFileFilter> fileFilter;
    std::unique_ptr<juce::DirectoryContentsList> dirContents;
    std::unique_ptr<juce::FileListComponent> fileList;
};