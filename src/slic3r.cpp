#include "Config.hpp"
#include "Geometry.hpp"
#include "Model.hpp"
#include "TriangleMesh.hpp"
#include "libslic3r.h"
#include <cstdio>
#include <string>
#include <cstring>
#include <iostream>
#include <math.h>
#include <boost/filesystem.hpp>
#include <boost/nowide/args.hpp>
#include <boost/nowide/iostream.hpp>

#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/GUI_App.hpp"

using namespace Slic3r;

// wxWidgets "Hello world" Program
// For compilers that support precompilation, includes "wx/wx.h".
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif
class MyApp: public wxApp
{
public:
    virtual bool OnInit();
};
class MyFrame: public wxFrame
{
public:
    MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
private:
    void OnHello(wxCommandEvent& event);
    void OnExit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    wxDECLARE_EVENT_TABLE();
};
enum
{
    ID_Hello = 1
};
wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
    EVT_MENU(ID_Hello,   MyFrame::OnHello)
    EVT_MENU(wxID_EXIT,  MyFrame::OnExit)
    EVT_MENU(wxID_ABOUT, MyFrame::OnAbout)
wxEND_EVENT_TABLE()
bool MyApp::OnInit()
{
    MyFrame *frame = new MyFrame( "Hello World", wxPoint(50, 50), wxSize(450, 340) );
    frame->Show( true );
    return true;
}
MyFrame::MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
        : wxFrame(NULL, wxID_ANY, title, pos, size)
{
    wxMenu *menuFile = new wxMenu;
    menuFile->Append(ID_Hello, "&Hello...\tCtrl-H",
                     "Help string shown in status bar for this menu item");
    menuFile->AppendSeparator();
    menuFile->Append(wxID_EXIT);
    wxMenu *menuHelp = new wxMenu;
    menuHelp->Append(wxID_ABOUT);
    wxMenuBar *menuBar = new wxMenuBar;
    menuBar->Append( menuFile, "&File" );
    menuBar->Append( menuHelp, "&Help" );
    SetMenuBar( menuBar );
    CreateStatusBar();
    SetStatusText( "Welcome to wxWidgets!" );
    Slic3r::Model model;
    ModelObject *object = model.add_object();
    SetStatusText(Slic3r::GUI::from_u8("HHuhuh"));
}

void MyFrame::OnExit(wxCommandEvent& event)
{
    Close( true );
}
void MyFrame::OnAbout(wxCommandEvent& event)
{
    wxMessageBox( "This is a wxWidgets' Hello world sample",
                  "About Hello World", wxOK | wxICON_INFORMATION );
}
void MyFrame::OnHello(wxCommandEvent& event)
{
    wxLogMessage("Hello world from wxWidgets!");
}


#if 1
int
main(int argc, char **argv)
{
    // Convert arguments to UTF-8 (needed on Windows).
    // argv then points to memory owned by a.
    boost::nowide::args a(argc, argv);
    
#if 0
    // parse all command line options into a DynamicConfig
    ConfigDef config_def;
    config_def.merge(cli_config_def);
    config_def.merge(print_config_def);
    DynamicConfig config(&config_def);
    t_config_option_keys input_files;
    config.read_cli(argc, argv, &input_files);
    
    // apply command line options to a more handy CLIConfig
    CLIConfig cli_config;
    cli_config.apply(config, true);
    
    DynamicPrintConfig print_config;
    
    // load config files supplied via --load
    for (const std::string &file : cli_config.load.values) {
        if (!boost::filesystem::exists(file)) {
            boost::nowide::cout << "No such file: " << file << std::endl;
            exit(1);
        }
        
        DynamicPrintConfig c;
        try {
            c.load(file);
        } catch (std::exception &e) {
            boost::nowide::cout << "Error while reading config file: " << e.what() << std::endl;
            exit(1);
        }
        c.normalize();
        print_config.apply(c);
    }
    
    // apply command line options to a more specific DynamicPrintConfig which provides normalize()
    // (command line options override --load files)
    print_config.apply(config, true);
    print_config.normalize();
    
    // write config if requested
    if (!cli_config.save.value.empty()) print_config.save(cli_config.save.value);
    
    // read input file(s) if any
    std::vector<Model> models;
    for (const t_config_option_key &file : input_files) {
        if (!boost::filesystem::exists(file)) {
            boost::nowide::cerr << "No such file: " << file << std::endl;
            exit(1);
        }
        
        Model model;
        try {
            model = Model::read_from_file(file);
        } catch (std::exception &e) {
            boost::nowide::cerr << file << ": " << e.what() << std::endl;
            exit(1);
        }
        
        if (model.objects.empty()) {
            boost::nowide::cerr << "Error: file is empty: " << file << std::endl;
            continue;
        }
        
        model.add_default_instances();
        
        // apply command line transform options
        for (ModelObject* o : model.objects) {
            if (cli_config.scale_to_fit.is_positive_volume())
                o->scale_to_fit(cli_config.scale_to_fit.value);
            
            // TODO: honor option order?
            o->scale(cli_config.scale.value);
            o->rotate(Geometry::deg2rad(cli_config.rotate_x.value), X);
            o->rotate(Geometry::deg2rad(cli_config.rotate_y.value), Y);
            o->rotate(Geometry::deg2rad(cli_config.rotate.value), Z);
        }
        
        // TODO: handle --merge
        models.push_back(model);
    }
    
    for (Model &model : models) {
        if (cli_config.info) {
            // --info works on unrepaired model
            model.print_info();
        } else if (cli_config.export_obj) {
            std::string outfile = cli_config.output.value;
            if (outfile.empty()) outfile = model.objects.front()->input_file + ".obj";
    
            TriangleMesh mesh = model.mesh();
            mesh.repair();
            IO::OBJ::write(mesh, outfile);
            boost::nowide::cout << "File exported to " << outfile << std::endl;
        } else if (cli_config.export_pov) {
            std::string outfile = cli_config.output.value;
            if (outfile.empty()) outfile = model.objects.front()->input_file + ".pov";
    
            TriangleMesh mesh = model.mesh();
            mesh.repair();
            IO::POV::write(mesh, outfile);
            boost::nowide::cout << "File exported to " << outfile << std::endl;
        } else if (cli_config.export_svg) {
            std::string outfile = cli_config.output.value;
            if (outfile.empty()) outfile = model.objects.front()->input_file + ".svg";
            
            SLAPrint print(&model);
            print.config.apply(print_config, true);
            print.slice();
            print.write_svg(outfile);
            boost::nowide::cout << "SVG file exported to " << outfile << std::endl;
        } else if (cli_config.cut_x > 0 || cli_config.cut_y > 0 || cli_config.cut > 0) {
            model.repair();
            model.translate(0, 0, -model.bounding_box().min.z);
            
            if (!model.objects.empty()) {
                // FIXME: cut all objects
                Model out;
                if (cli_config.cut_x > 0) {
                    model.objects.front()->cut(X, cli_config.cut_x, &out);
                } else if (cli_config.cut_y > 0) {
                    model.objects.front()->cut(Y, cli_config.cut_y, &out);
                } else {
                    model.objects.front()->cut(Z, cli_config.cut, &out);
                }
                
                ModelObject &upper = *out.objects[0];
                ModelObject &lower = *out.objects[1];
            
                if (upper.facets_count() > 0) {
                    TriangleMesh m = upper.mesh();
                    IO::STL::write(m, upper.input_file + "_upper.stl");
                }
                if (lower.facets_count() > 0) {
                    TriangleMesh m = lower.mesh();
                    IO::STL::write(m, lower.input_file + "_lower.stl");
                }
            }
        } else if (cli_config.cut_grid.value.x > 0 && cli_config.cut_grid.value.y > 0) {
            TriangleMesh mesh = model.mesh();
            mesh.repair();
            
            TriangleMeshPtrs meshes = mesh.cut_by_grid(cli_config.cut_grid.value);
            size_t i = 0;
            for (TriangleMesh* m : meshes) {
                std::ostringstream ss;
                ss << model.objects.front()->input_file << "_" << i++ << ".stl";
                IO::STL::write(*m, ss.str());
                delete m;
            }
        } else {
            boost::nowide::cerr << "error: command not supported" << std::endl;
            return 1;
        }
    }
#endif
    

//     MyApp *gui = new MyApp();
    GUI::GUI_App *gui = new GUI::GUI_App();

//     MyApp::SetInstance(gui);
    GUI::GUI_App::SetInstance(gui);

    wxEntry(argc, argv);
    return 0;
}
#endif
