#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <memory>
#include <cassert>
#include <param.h>
#include <grid.h>
#include <integrator.h>
#include <breg.h>
#include <brnd.h>
#include <cre.h>
#include <fereg.h>
#include <fernd.h>
#include <namespace_toolkit.h>
#include <tinyxml2.h>
#include <timer.h>

class Pipeline{
public:
    Pipeline () = default;
    Pipeline (const std::string &);
    virtual ~Pipeline () = default;
    void assemble_grid ();
    void assemble_fereg ();
    void assemble_breg ();
    void assemble_fernd ();
    void assemble_brnd ();
    void assemble_cre ();
    void assemble_obs ();
private:
    std::unique_ptr<Param> par;
    std::unique_ptr<Grid_fereg> grid_fereg;
    std::unique_ptr<Grid_breg> grid_breg;
    std::unique_ptr<Grid_brnd> grid_brnd;
    std::unique_ptr<Grid_fernd> grid_fernd;
    std::unique_ptr<Grid_cre> grid_cre;
    std::unique_ptr<Grid_int> grid_int;
    std::unique_ptr<FEreg> fereg;
    std::unique_ptr<Breg> breg;
    std::unique_ptr<FErnd> fernd;
    std::unique_ptr<Brnd> brnd;
    std::unique_ptr<CRE> cre;
    std::unique_ptr<Integrator> intobj;
};

// constructor
Pipeline::Pipeline (const std::string &filename){
    par = std::make_unique<Param> (filename);
}

void Pipeline::assemble_grid (){
    grid_fereg = std::make_unique<Grid_fereg> (par.get());
    grid_breg = std::make_unique<Grid_breg> (par.get());
    grid_brnd = std::make_unique<Grid_brnd> (par.get());
    grid_fernd = std::make_unique<Grid_fernd> (par.get());
    grid_cre = std::make_unique<Grid_cre> (par.get());
    grid_int = std::make_unique<Grid_int> (par.get());
}

// regular FE field
void Pipeline::assemble_fereg (){
    if (!par->grid_fereg.build_permission){
        fereg = std::make_unique<FEreg> ();
        return;
    }
    // if import from file, no need to build specific fe class
    if (par->grid_fereg.read_permission){
        grid_fereg->import_grid (par.get());
        fereg = std::make_unique<FEreg> ();
    }
    else if (par->fereg_type=="YMW16"){
        fereg = std::make_unique<FEreg_ymw16> ();
    }
#ifndef NDEBUG
    else if (par->fereg_type=="Test"){
        fereg = std::make_unique<FEreg_test> ();
    }
#endif
    else assert (false);
    // if export to file
    if (par->grid_fereg.write_permission){
        // write out binary file and exit
        fereg->write_grid (par.get(),grid_fereg.get());
        grid_fereg->export_grid (par.get());
    }
}

// regular B field, must before random B field
void Pipeline::assemble_breg (){
    if (!par->grid_breg.build_permission){
        breg = std::make_unique<Breg> ();
        return;
    }
    if (par->grid_breg.read_permission){
        grid_breg->import_grid (par.get());
        breg = std::make_unique<Breg> ();
    }
    else if (par->breg_type=="WMAP"){
        breg = std::make_unique<Breg_wmap> ();
    }
    else if (par->breg_type=="Jaffe"){
        breg = std::make_unique<Breg_jaffe> ();
    }
#ifndef NDEBUG
    else if (par->breg_type=="Test"){
        breg = std::make_unique<Breg_test> ();
    }
#endif
    else assert (false);
    // if export to file
    if (par->grid_breg.write_permission){
        breg->write_grid (par.get(),grid_breg.get());
        grid_breg->export_grid (par.get());
    }
}

// random FE field
void Pipeline::assemble_fernd (){
    // if import from file, no need to build specific fe_rnd class
    if (par->grid_fernd.read_permission){
        grid_fernd->import_grid (par.get());
        fernd = std::make_unique<FErnd> ();
    }
    else if (par->grid_fernd.build_permission){
        if (par->fernd_type=="Global"){
            if (par->fernd_method=="DFT"){
                fernd = std::make_unique<FErnd_dft> ();
            }
            // fill grid with random fields
            fernd->write_grid (par.get(),grid_fernd.get());
        }
        else assert (false);
    }
    else {
        fernd = std::make_unique<FErnd> ();
    }
    // if export to file
    if (par->grid_fernd.write_permission){
        grid_fernd->export_grid (par.get());
    }
}

// random B field
void Pipeline::assemble_brnd (){
    if (par->grid_brnd.read_permission){
        grid_brnd->import_grid (par.get());
        brnd = std::make_unique<Brnd> ();
    }
    else if (par->grid_brnd.build_permission){
        if (par->brnd_type=="Global"){
            if (par->brnd_method=="ES"){
                brnd = std::make_unique<Brnd_es> ();
            }
            /*
            else if (par->brnd_method=="Jaffe"){
                brnd = std::make_unique<Brnd_jaffe> ();
            }
             */
            // fill grid with random fields
            brnd->write_grid (par.get(),
                              breg.get(),
                              grid_breg.get(),
                              grid_brnd.get());
        }
        else if (par->brnd_type=="Local"){
            if (par->brnd_method=="MHD"){
                brnd = std::make_unique<Brnd_mhd> ();
            }
            // fill grid with random fields
            brnd->write_grid (par.get(),
                              breg.get(),
                              grid_breg.get(),
                              grid_brnd.get());
        }
        else assert (false);
    }
    else {
        //without read permission, return zeros
        brnd = std::make_unique<Brnd> ();
    }
    // if export to file
    if (par->grid_brnd.write_permission){
        grid_brnd->export_grid (par.get());
    }
}

// cre
void Pipeline::assemble_cre (){
    if (par->cre_type=="Analytic"){
        cre = std::make_unique<CRE_ana> ();
    }
#ifndef NDEBUG
    else if (par->cre_type=="Test"){
        cre = std::make_unique<CRE_test>();
    }
#endif
    else if (par->cre_type=="Numeric"){
        grid_cre->import_grid (par.get());
        cre = std::make_unique<CRE_num> ();
    }
    else assert (false);
    // if export to file
    if (par->grid_cre.write_permission){
        cre->write_grid (par.get(),
                         grid_cre.get());
        grid_cre->export_grid (par.get());
    }
}
// LOS integration for observables
void Pipeline::assemble_obs (){
    intobj = std::make_unique<Integrator> ();
    for (unsigned int i=0;i<par->grid_int.do_sync.size();++i){
        if (i>0) {
            par->grid_int.do_dm = false;
            par->grid_int.do_fd = false;
        }
        intobj->write_grid (breg.get(),
                            brnd.get(),
                            fereg.get(),
                            fernd.get(),
                            cre.get(),
                            grid_breg.get(),
                            grid_brnd.get(),
                            grid_fereg.get(),
                            grid_fernd.get(),
                            grid_cre.get(),
                            grid_int.get(),
                            par.get());
        grid_int->export_grid (par.get());
        par->grid_int.do_sync.pop_back ();
        par->grid_int.sim_sync_freq.pop_back ();
        par->grid_int.sim_sync_name.pop_back ();
    }
}

int main (int /*argc*/,char **argv) {
#ifndef NTIMING
    auto tmr = std::make_unique<Timer> ();
    tmr->start ("main");
#endif
    const std::string input(argv[1]);
    auto run = std::make_unique<Pipeline> (input);
    run->assemble_grid ();
    run->assemble_fereg ();
    run->assemble_breg ();
    run->assemble_fernd ();
    run->assemble_brnd ();
    run->assemble_cre ();
    run->assemble_obs ();
#ifndef NTIMING
    tmr->stop ("main");
    tmr->print();
#endif 
    return EXIT_SUCCESS;
}

// END