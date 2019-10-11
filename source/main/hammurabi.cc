#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <bfield.h>
#include <crefield.h>
#include <grid.h>
#include <integrator.h>
#include <param.h>
#include <tefield.h>
#include <timer.h>
#include <toolkit.h>

class Pipeline {
public:
  Pipeline() = default;
  Pipeline(const std::string &);
  Pipeline(const Pipeline &) = delete;
  Pipeline(Pipeline &&) = delete;
  Pipeline &operator=(const Pipeline &) = delete;
  Pipeline &operator=(Pipeline &&) = delete;
  virtual ~Pipeline() = default;
  void assemble_grid();
  void assemble_tereg();
  void assemble_breg();
  void assemble_ternd();
  void assemble_brnd();
  void assemble_cre();
  void assemble_obs();

private:
  std::unique_ptr<Param> par;
  std::unique_ptr<Grid_tereg> grid_tereg;
  std::unique_ptr<Grid_breg> grid_breg;
  std::unique_ptr<Grid_brnd> grid_brnd;
  std::unique_ptr<Grid_ternd> grid_ternd;
  std::unique_ptr<Grid_cre> grid_cre;
  std::unique_ptr<Grid_obs> grid_obs;
  std::unique_ptr<TEreg> tereg;
  std::unique_ptr<Breg> breg;
  std::unique_ptr<TErnd> ternd;
  std::unique_ptr<Brnd> brnd;
  std::unique_ptr<CREfield> cre;
  std::unique_ptr<Integrator> intobj;
};

// constructor
Pipeline::Pipeline(const std::string &filename) {
  par = std::make_unique<Param>(filename);
}

void Pipeline::assemble_grid() {
  grid_tereg = std::make_unique<Grid_tereg>(par.get());
  grid_breg = std::make_unique<Grid_breg>(par.get());
  grid_brnd = std::make_unique<Grid_brnd>(par.get());
  grid_ternd = std::make_unique<Grid_ternd>(par.get());
  grid_cre = std::make_unique<Grid_cre>(par.get());
  grid_obs = std::make_unique<Grid_obs>(par.get());
}

// regular thermel electron field
void Pipeline::assemble_tereg() {
  if (!par->grid_tereg.build_permission) {
    tereg = std::make_unique<TEreg>();
    return;
  }
  // if import from file, no need to build specific fe class
  if (par->grid_tereg.read_permission) {
    grid_tereg->import_grid(par.get());
    tereg = std::make_unique<TEreg>();
  } else if (par->tereg_type == "ymw16") {
    tereg = std::make_unique<TEreg_ymw16>();
  } else if (par->tereg_type == "unif") {
    tereg = std::make_unique<TEreg_unif>();
  } else
    throw std::runtime_error("unsupported tereg model");
  // if export to file
  if (par->grid_tereg.write_permission) {
    // write out binary file and exit
    tereg->write_grid(par.get(), grid_tereg.get());
    grid_tereg->export_grid(par.get());
  }
}

// regular magnetic field
void Pipeline::assemble_breg() {
  if (!par->grid_breg.build_permission) {
    breg = std::make_unique<Breg>();
    return;
  }
  if (par->grid_breg.read_permission) {
    grid_breg->import_grid(par.get());
    breg = std::make_unique<Breg>();
  } else if (par->breg_type == "lsa") {
    breg = std::make_unique<Breg_lsa>();
  } else if (par->breg_type == "jaffe") {
    breg = std::make_unique<Breg_jaffe>();
  } else if (par->breg_type == "unif") {
    breg = std::make_unique<Breg_unif>();
  } else
    throw std::runtime_error("unsupported breg model");
  // if export to file
  if (par->grid_breg.write_permission) {
    breg->write_grid(par.get(), grid_breg.get());
    grid_breg->export_grid(par.get());
  }
}

// random thermal electron field
void Pipeline::assemble_ternd() {
  // if import from file, no need to build specific fe_rnd class
  if (par->grid_ternd.read_permission) {
    grid_ternd->import_grid(par.get());
    ternd = std::make_unique<TErnd>();
  } else if (par->grid_ternd.build_permission) {
    if (par->ternd_type == "global") {
      if (par->ternd_method == "dft") {
        ternd = std::make_unique<TErnd_dft>();
      }
      // fill grid with random fields
      ternd->write_grid(par.get(), grid_ternd.get());
    } else
      throw std::runtime_error("unsupported brnd model");
  } else {
    ternd = std::make_unique<TErnd>();
  }
  // if export to file
  if (par->grid_ternd.write_permission) {
    grid_ternd->export_grid(par.get());
  }
}

// random magnetic field
void Pipeline::assemble_brnd() {
  if (par->grid_brnd.read_permission) {
    grid_brnd->import_grid(par.get());
    brnd = std::make_unique<Brnd>();
  } else if (par->grid_brnd.build_permission) {
    if (par->brnd_type == "global") {
      if (par->brnd_method == "es") {
        brnd = std::make_unique<Brnd_es>();
      } else
        throw std::runtime_error("unsupported brnd model");
      // fill grid with random fields
      brnd->write_grid(par.get(), breg.get(), grid_breg.get(), grid_brnd.get());
    } else if (par->brnd_type == "local") {
      if (par->brnd_method == "mhd") {
        brnd = std::make_unique<Brnd_mhd>();
      }
      // fill grid with random fields
      brnd->write_grid(par.get(), breg.get(), grid_breg.get(), grid_brnd.get());
    } else
      throw std::runtime_error("unsupported brnd model");
  } else {
    // without read permission, return zeros
    brnd = std::make_unique<Brnd>();
  }
  // if export to file
  if (par->grid_brnd.write_permission) {
    grid_brnd->export_grid(par.get());
  }
}

// cre flux field
void Pipeline::assemble_cre() {
  if (par->grid_cre.read_permission) {
    grid_cre->import_grid(par.get());
    cre = std::make_unique<CRE_num>();
  } else if (par->grid_cre.build_permission) {
    if (par->cre_type == "analytic") {
      cre = std::make_unique<CRE_ana>();
    } else if (par->cre_type == "unif") {
      cre = std::make_unique<CRE_unif>();
    } else
      throw std::runtime_error("unsupported cre model");
  }
  // if export to file
  if (par->grid_cre.write_permission) {
    cre->write_grid(par.get(), grid_cre.get());
    grid_cre->export_grid(par.get());
  }
}

// LoS integration for observables
void Pipeline::assemble_obs() {
  intobj = std::make_unique<Integrator>();
  if (par->grid_obs.write_permission) {
    const auto repeat = par->grid_obs.do_sync.size();
    for (unsigned int i = 0; i < repeat; ++i) {
      if (i > 0) {
        par->grid_obs.do_dm = false;
        par->grid_obs.do_fd = false;
        // need to rebuild integration grid
        grid_obs->build_grid(par.get());
      }
      intobj->write_grid(breg.get(), brnd.get(), tereg.get(), ternd.get(),
                         cre.get(), grid_breg.get(), grid_brnd.get(),
                         grid_tereg.get(), grid_ternd.get(), grid_cre.get(),
                         grid_obs.get(), par.get());
      grid_obs->export_grid(par.get());
      // delete obsolete parameters
      if (par->grid_obs.do_sync.back()) {
        par->grid_obs.nside_sync.pop_back();
        par->grid_obs.do_sync.pop_back();
        par->grid_obs.sim_sync_freq.pop_back();
        par->grid_obs.sim_sync_name.pop_back();
      }
    }
  }
}

int main(int /*argc*/, char **argv) {
#ifndef NTIMING
  auto tmr = std::make_unique<Timer>();
  tmr->start("main");
#endif
  const std::string input(argv[1]);
  auto run = std::make_unique<Pipeline>(input);
  run->assemble_grid();
  run->assemble_tereg();
  run->assemble_breg();
  run->assemble_ternd();
  run->assemble_brnd();
  run->assemble_cre();
  run->assemble_obs();
#ifndef NTIMING
  tmr->stop("main");
  tmr->print();
#endif
  return EXIT_SUCCESS;
}
