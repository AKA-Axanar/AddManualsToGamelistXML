
// AddManualsToGamelistXML.cpp : by Steve Simpson (A.K.A. Axanar)
// Copyright 2024 by Stephen Simpson, (Steve Simpson, A.K.A. Axanar)
// under the MIT Open Source license.  See License.txt.

// This program reads the roms sub-directories for gamelist.xml files and checks for the existence of a manual tag in the xml file.
// If the manual tag does not exist and there is a manual file in the media/manuals directory with the same name as the game.
// it adds a <manual> xml tag to the xml file.  If it doesn't find a manual file it writes the game name to missing_manuals.txt.  
// Run this program in the roms directory.

// Skraper can scrape game manuals but for some reason it doesn't add
// the path to the manual as an xml tag in gamelist.xml.  If you have
// scraped manuals with Skraper you can run this app in the roms
// folder to add them all to the gamelist.xml files.
// You only need to do this once until you add more games and run 
// Skraper again.

// A -r option will remove the manual tags if it points to media/manuals
// This is useful to test the add function or add again from sc
// -r will not remove any manual tags that do not point to the media/manuals director
// as the add routine will only add manual tags back in that are in the media/manuals directory
// This is to prevent removing manual tags that were added manually or in some other way
// For example the manual tags in PICOwesome point to a different directory and the gamelist.xm
// is created using his own program specially made for PICOwesome

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include "pugixml.hpp"
#include "pugiconfig.hpp"
#include <ranges>
#include <functional>
#include <regex>

using namespace std;
using Strings = std::vector<std::string>;
namespace fs = std::filesystem;
using namespace pugi;

//
// the directory, file, and string routines are copied from the TauLib library that I use in TauBleem
//

///////////////////////

//
// does file or directory exist
//
bool FileExists(const string& filePath) {
    return fs::exists(filePath) && fs::is_regular_file(filePath);
} 
bool DirExists(const string& dirPath) {
    return fs::exists(dirPath) && fs::is_directory(dirPath);
}

//
// return directory contents as strings of file or dir names
// takes lambdas to control what gets returned
//
Strings GetDirectoryContents(const string& dirPath, 
                             function<bool (fs::directory_entry&)> testLambda,
                             function<string (fs::directory_entry&)> getStringLambda,
                             bool recursive
                             )
{
    Strings result;
    if (!DirExists(dirPath))
        return result;

    if (recursive) {
        for (auto dir_entry : fs::recursive_directory_iterator(dirPath)) {
            if (testLambda(dir_entry)) {
                result.emplace_back(getStringLambda(dir_entry));
            }
        }
    } else {
        for (auto dir_entry : fs::directory_iterator(dirPath)) {
            if (testLambda(dir_entry)) {
                result.emplace_back(getStringLambda(dir_entry));
            }
        }
    }

    return result;
}

/// @brief lambda to return if a directory_entry is a directory
inline auto is_directory = [] (fs::directory_entry& entry)->bool { return entry.is_directory(); };

/// @brief lambda to return the file name or dir name portion of a directory_entry
inline auto get_name =     [] (fs::directory_entry& entry)->std::string { return entry.path().filename().string(); };

//
// GetDirNamesInDir
//
Strings GetDirNamesInDir(const std::string& dirPath)
    { return GetDirectoryContents(dirPath, is_directory, get_name, false); }

//
// returns the base/stem of the filename.  ex: returns "foo" from "aaa/bbb/foo.dat".  i.e. return the filename without the extension
// If the path is a directory it returns the last part of the dir path.  ex: returns "ccc" from "aaa/bbb/ccc"
//
string GetFilenameBase(const string& str) {
    fs::path p(str);
    return p.stem().string();   
}

//
// FoundLexExpr - returns whether the lexical expression (or plain string) is found in a string.
// @param lexicalExpressionOrString The string or lexical expression to look for in the string.
// ex: lexical expression = "[A-Za-z0-9]+"
// ex: lexical expression = "FindMe"
// return true or false
//
bool FoundLexExpr(const string& lexicalExpressionOrString, const string& str) {
    regex expr(lexicalExpressionOrString);
    return regex_search(str, expr);
}

///////////////////////

void AddManualsToGamelistXML()
{
    int countAdded {0};                     // number of manuals added
    int countExisting {0};                  // number of manual key that already exist
    int countMissingManual {0};             // number of no manual found

    ofstream missingManuals("missing_manuals.txt");
    Strings romDirs = GetDirNamesInDir(".");
    ranges::for_each(romDirs, [&] (const string& romDir) {

        string xmlfilename = romDir + "/" + "gamelist.xml";
        string manualsPath = romDir + "/" + "media" + "/" + "manuals";
        if (FileExists(xmlfilename) && DirExists(manualsPath)) {
            xml_document xmldoc;
            if (xmldoc.load_file(xmlfilename.c_str())) {
                bool xmlDataModified = false;
                xml_node gamelist = xmldoc.child("gameList");
                if (gamelist) {
                    // get each game node info
                    for (xml_node game_node = gamelist.child("game"); game_node != nullptr; game_node = game_node.next_sibling("game")) {
                        string game_path = game_node.child_value("path");
                        if (game_path.empty())
                            continue;   // no game path
                        string manual_path = game_node.child_value("manual");
                        if (!manual_path.empty()) {
                            cout << "Manual tag already exists for " + romDir + "/" + game_path << endl;
                            ++countExisting;
                            continue;   // already has a manual xml tag
                        }

                        // see if there is a manual with a pdf or txt extension
                        auto TryExtension = [&] (const string& ext) {
                            if (manual_path.empty()) {
                                manual_path = "./media/manuals/" + GetFilenameBase(game_path) + ext;
                                if (!FileExists(romDir + "/" + manual_path)) {
                                    manual_path = "";   // not found
                                }
                            }
                        };

                        if (manual_path.empty())
                            TryExtension(".pdf");
                        if (manual_path.empty())
                            TryExtension(".txt");

                        if (!manual_path.empty()) {
                            pugi::xml_node manual_node = game_node.append_child("manual");
                            manual_node.append_child(pugi::node_pcdata).set_value(manual_path.c_str());
                            cout << "Added manual for " + romDir + "/" + game_path << endl;
                            ++countAdded;
                            cout << "Added manual for " + romDir + "/" + game_path << endl;
                            xmlDataModified = true;
                        }
                        else {
                            cout << "No manual found for " + romDir + "/" + game_path << endl;;
                            missingManuals << romDir + "/" + game_path << endl;
                            ++countMissingManual;
                        }
                    }
                }
                if (xmlDataModified)
                    xmldoc.save_file(xmlfilename.c_str());
            }
        }
    });
    cout << endl;
    cout << to_string(countAdded) + " Manuals added to gamelist.xml" << endl;
    cout << to_string(countExisting) + " Existing xml manual tags in gamelist.xml" << endl;
    cout << to_string(countMissingManual) + " Missing manual files" << endl;
    missingManuals.close();
}

///////////////////////

void RemoveManualsFromGamelistXML()
{
    int countRemoved {0};                   // number of manuals removed

    Strings romDirs = GetDirNamesInDir(".");
    ranges::for_each(romDirs, [&] (const string& romDir) {

        string xmlfilename = romDir + "/" + "gamelist.xml";
        string manualsPath = romDir + "/" + "media" + "/" + "manuals";
        if (FileExists(xmlfilename) && DirExists(manualsPath)) {
            xml_document xmldoc;
            if (xmldoc.load_file(xmlfilename.c_str())) {
                bool xmlDataModified = false;
                xml_node gamelist = xmldoc.child("gameList");
                if (gamelist) {
                    // get each game node info
                    for (xml_node game_node = gamelist.child("game"); game_node != nullptr; game_node = game_node.next_sibling("game")) {
                        string game_path = game_node.child_value("path");
                        if (game_path.empty())
                            continue;   // no game path
                        string manual_path = game_node.child_value("manual");
                        if (FoundLexExpr("media/manuals", manual_path)) {
                            game_node.remove_child("manual");
                            cout << "Manual tag removed for " + romDir + "/" + game_path << endl;
                            xmlDataModified = true;
                            ++countRemoved;
                            continue;   // already has a manual xml tag
                        }
                        else {
                            // Do not remove any manual tags that do not point to the media/manuals directory
                            // as the add routine will only add manual tags back in that are in the media/manuals directory.
                            // This is to prevent removing manual tags that were added manually or in some other way.
                            // For example the manual tags in PICOwesome point to a different directory and the gamelist.xml
                            // is created using his own program specially made for PICOwesome.
                        }
                    }
                }
                if (xmlDataModified)
                    xmldoc.save_file(xmlfilename.c_str());
            }
        }
    });
    cout << endl;
    cout << to_string(countRemoved) + " Manuals removed from gamelist.xml" << endl;
}

int main(int argc, char* argv[])
{
    cout << "AddManualsToGamelistXML : by Steve Simpson (A.K.A. Axanar)" << endl;
    cout << "This program reads the roms sub-directories for gamelist.xml files and checks for the existence of a manual tag in the xml file." <<endl;
    cout << "If the manual tag does not exist and there is a manual file in the media/manuals directory with the same name as the game" <<endl;
    cout << "it adds a <manual> xml tag to the xml file.  If it doesn't find a manual file it writes the game name to missing_manuals.txt." <<endl;
    cout << "Run this program in the roms directory." <<endl;
    cout << endl;

    cout << "Skraper can scrape manuals but for some reason it doesn't add" <<endl;
    cout << "the path to the manual as an xml tag in gamelist.xml.  If you have" <<endl;
    cout << "scraped manuals with Skraper you can run this app in the roms" <<endl;
    cout << "folder to add them all to the gamelist.xml files." <<endl;
    cout << "You only need to do this once until you add more games and run " <<endl;
    cout << "Skraper again." <<endl;
    cout << endl;

    cout << "A -r option will remove the manual tags if it points to media/manuals." <<endl;
    cout << "This is useful to test the add function or add again from scratch." <<endl;
    cout << endl;
    cout << "-r will not remove any manual tags that do not point to the media/manuals directory" <<endl;
    cout << "as the add routine will only add manual tags back in that are in the media/manuals directory." <<endl;
    cout << "This is to prevent removing manual tags that were added manually or in some other way." <<endl;
    cout << "For example the manual tags in PICOwesome point to a different directory and the gamelist.xml" <<endl;
    cout << "is created using his own program specially made for PICOwesome." <<endl;
    cout << endl;

    if (argc == 2 && (_stricmp(argv[1], "-r") == 0))
        RemoveManualsFromGamelistXML();
    else
        AddManualsToGamelistXML();
    return 0;
}
