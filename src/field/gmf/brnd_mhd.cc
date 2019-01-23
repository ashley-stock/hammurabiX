#include <cmath>
#include <cassert>
#include <omp.h>

#include <vec3.h>
#include <fftw3.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_rng.h>

#include <param.h>
#include <grid.h>
#include <brnd.h>
#include <breg.h>
#include <cgs_units_file.h>
#include <namespace_toolkit.h>

void Brnd_mhd::write_grid (const Param *par,
                           const Breg *breg,
                           const Grid_breg *gbreg,
                           Grid_brnd *grid) const{
    // initialize random seed
#ifdef _OPENMP
    gsl_rng **threadvec = new gsl_rng *[omp_get_max_threads()];
    for (int b=0;b<omp_get_max_threads();++b){
        threadvec[b] = gsl_rng_alloc(gsl_rng_taus);
        gsl_rng_set(threadvec[b],b+toolkit::random_seed(par->brnd_seed));
    }
#else
    gsl_rng *r {gsl_rng_alloc(gsl_rng_taus)};
    gsl_rng_set(r, toolkit::random_seed(par->brnd_seed));
#endif
    // start Fourier space filling
    const double lx {par->grid_brnd.x_max-par->grid_brnd.x_min};
    const double ly {par->grid_brnd.y_max-par->grid_brnd.y_min};
    const double lz {par->grid_brnd.z_max-par->grid_brnd.z_min};
    const vec3_t<double> B {breg->get_breg(par->SunPosition,par,gbreg)};
    // physical dk^3
    const double dk3 {CGS_U_kpc*CGS_U_kpc*CGS_U_kpc/(lx*ly*lz)};
#ifdef _OPENMP
#pragma omp parallel for schedule(static) // DO NOT CHANGE SCHEDULE TYPE
#endif
    for (decltype(par->grid_brnd.nx) i=0;i<par->grid_brnd.nx;++i) {
#ifdef _OPENMP
        auto seed_id = threadvec[omp_get_thread_num()];
#else
        auto seed_id = r;
#endif
        vec3_t<double> k {CGS_U_kpc*i/lx,0,0};
        if (i>=(par->grid_brnd.nx+1)/2) k.x -= CGS_U_kpc*par->grid_brnd.nx/lx;
        // it's better to calculate indeces manually
        // just for reference, how indeces are calculated
        // const size_t idx {toolkit::index3d(par->grid_brnd.nx,par->grid_brnd.ny,par->grid_brnd.nz,i,j,l)};
        const std::size_t idx_lv1 {i*par->grid_brnd.ny*par->grid_brnd.nz};
        for (decltype(par->grid_brnd.ny) j=0;j<par->grid_brnd.ny;++j) {
            k.y = CGS_U_kpc*j/ly;
            if (j>=(par->grid_brnd.ny+1)/2) k.y -= CGS_U_kpc*par->grid_brnd.ny/ly;
            const std::size_t idx_lv2 {idx_lv1+j*par->grid_brnd.nz};
            for (decltype(par->grid_brnd.nz) l=0;l<par->grid_brnd.nz;++l) {
                // the very 0th term is fixed to zero in allocation
                if (i==0 and j==0 and l==0) continue;
                k.z = CGS_U_kpc*l/lz;
                if (l>=(par->grid_brnd.nz+1)/2) k.z -= CGS_U_kpc*par->grid_brnd.nz/lz;
                const double ks {k.Length()};
                const std::size_t idx {idx_lv2+l};
                vec3_t<double> ep {eplus(B,k)};
                vec3_t<double> em {eminus(B,k)};
                // since there is no specific rule about how to allocate spectrum power
                // between Re and Im part in b+ and b-
                // we multiply power by two and set Im parts to zero
                // in the end of backward trans, we retrive Re part of bx,by,bz only
                if (ep.SquaredLength()>1e-6){
                    double ang {cosa(B,k)};
                    const double Pa {speca(ks,par)*fa(par->brnd_mhd.ma,ang)*dk3};
                    double Pf {specf(ks,par)*hf(par->brnd_mhd.beta,ang)*dk3};
                    double Ps {specs(ks,par)*fs(par->brnd_mhd.ma,ang)*hs(par->brnd_mhd.beta,ang)*dk3};
                    // b+ is independent from b- in terms of power
                    // fast and slow modes are independent
                    const double Ap = gsl_ran_ugaussian(seed_id)*std::sqrt(2.*Pa);
                    const double Am = gsl_ran_ugaussian(seed_id)*std::sqrt(2.*Pf) + gsl_ran_ugaussian(seed_id)*std::sqrt(2.*Ps);
                    const vec3_t<double> bkp {ep*Ap};
                    const vec3_t<double> bkm {em*Am};
                    // c0_R = bx_Re - by_Im
                    // c0_I = bx_Im + by_Re
                    // c1_R = by_Re - bz_Im
                    // c1_I = by_Im + bz_Re
                    // bx_R = bkp_Re.x+bkm_Re.x;
                    // etc.
                    // note that all Im parts are zero, c1_I = c0_R
                    grid->c0[idx][0] = bkp.x + bkm.x;
                    grid->c0[idx][1] = bkp.y + bkm.y;
                    grid->c1[idx][0] = bkp.y + bkm.y;
                    grid->c1[idx][1] = bkp.z + bkm.z;
                }
                else {
                    double Pf {specf(ks,par)*hf(par->brnd_mhd.beta,1)*dk3};
                    if (i==0 and j==0){
                        ep.x = k.x;
                        em.y = k.y;
                    }
                    else {
                        ep.x = k.x*k.z/(ks*std::sqrt(k.x*k.x+k.y*k.y));
                        ep.y = k.y*k.z/(ks*std::sqrt(k.x*k.x+k.y*k.y));
                        ep.z = -std::sqrt(k.x*k.x+k.y*k.y)/ks;
                        em.x = -k.y/std::sqrt(k.x*k.x+k.y*k.y);
                        em.y = k.x/std::sqrt(k.x*k.x+k.y*k.y);
                        em.z = 0.;
                    }
                    // b+ and b- share power
                    const double Af = gsl_ran_ugaussian(seed_id)*std::sqrt(2.*Pf);
                    const double share = gsl_rng_uniform(seed_id);
                    const vec3_t<double> bkp {ep*Af*share};
                    const vec3_t<double> bkm {em*Af*(1.-share)};
                    grid->c0[idx][0] = bkp.x + bkm.x;
                    grid->c0[idx][1] = bkp.y + bkm.y;
                    grid->c1[idx][0] = grid->c0[idx][1];
                    grid->c1[idx][1] = bkp.z + bkm.z;
                }
            }// l
        }// j
    }// i
    // free random memory
#ifdef _OPENMP
    for (int b=0;b<omp_get_max_threads();++b)
        gsl_rng_free(threadvec[b]);
    delete [] threadvec;
#else
    gsl_rng_free(r);
#endif
    // execute DFT backward plan
    fftw_execute_dft (grid->plan_c0_bw,grid->c0,grid->c0);
    fftw_execute_dft (grid->plan_c1_bw,grid->c1,grid->c1);
    // now we need to get real parts manually
    // since we start in k-space real fields
    // the results in x-space are complex fields
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (decltype(par->grid_brnd.nx) i=0;i<par->grid_brnd.nx;++i) {
        decltype(par->grid_brnd.nx) i_sym {par->grid_brnd.nx-i};// apply Hermitian symmetry
        if (i==0) i_sym = i;
        // it's better to calculate indeces manually
        // just for reference, how indeces are calculated
        // const std::size_t idx {toolkit::index3d(par->grid_brnd.nx,par->grid_brnd.ny,par->grid_brnd.nz,i,j,l)};
        // const std::size_t idx_sym {toolkit::index3d(par->grid_brnd.nx,par->grid_brnd.ny,par->grid_brnd.nz,i_sym,j_sym,l_sym)};
        const std::size_t idx_lv1 {i*par->grid_brnd.ny*par->grid_brnd.nz};
        const std::size_t idx_sym_lv1 {i_sym*par->grid_brnd.ny*par->grid_brnd.nz};
        for (decltype(par->grid_brnd.ny) j=0;j<par->grid_brnd.ny;++j) {
            decltype(par->grid_brnd.ny) j_sym {par->grid_brnd.ny-j};// apply Hermitian symmetry
            if (j==0) j_sym = j;
            const std::size_t idx_lv2 {idx_lv1+j*par->grid_brnd.nz};
            const std::size_t idx_sym_lv2 {idx_sym_lv1+j_sym*par->grid_brnd.nz};
            for (decltype(par->grid_brnd.nz) l=0;l<par->grid_brnd.nz;++l) {
                decltype(par->grid_brnd.nz) l_sym {par->grid_brnd.nz-l};// apply Hermitian symmetry
                if (l==0) l_sym = l;
                const std::size_t idx {idx_lv2+l}; //k
                const std::size_t idx_sym {idx_sym_lv2+l_sym}; //-k
                // reconstruct bx,by,bz from c0,c1,c*0,c*1
                // c0(k) = bx(k) + i by(k)
                // c*0(-k) = bx(k) - i by(k)
                // c1(k) = by(k) + i bz(k)
                // c1*1(-k) = by(k) - i bz(k)
                grid->bx[idx] = 0.5*(grid->c0[idx][0]+grid->c0[idx_sym][0]);
                grid->by[idx] = 0.5*(grid->c1[idx][0]+grid->c1[idx_sym][0]);
                grid->bz[idx] = 0.5*(grid->c1[idx_sym][1]+grid->c1[idx][1]);
            }
        }
    }
}

// PRIVATE FUNCTIONS FOR LOW-BETA SUB-ALFVENIC PLASMA
vec3_t<double> Brnd_mhd::eplus (const vec3_t<double> &b,
                                const vec3_t<double> &k) const{
    vec3_t<double> tmp {crossprod(toolkit::versor(k),toolkit::versor(b))};
    if (tmp.SquaredLength()<1e-12){
        return tmp;
    }
    else {
        return tmp/tmp.Length();
        }
}

vec3_t<double> Brnd_mhd::eminus (const vec3_t<double> &b,
                                 const vec3_t<double> &k) const{
    vec3_t<double> tmp {crossprod(crossprod(toolkit::versor(k),toolkit::versor(b)),toolkit::versor(k))};
    if (tmp.SquaredLength()<1e-12){
        return tmp;
    }
    else {
        return tmp/tmp.Length();
        }
}

double Brnd_mhd::hs (const double &beta,
                     const double &cosa) const{
    if (cosa<1e-6){
        return 0;
    }
    else {
        double dispersion {0.5*(1+0.5*beta)*( 1-std::sqrt(1-2*beta*cosa*cosa/((1+0.5*beta)*(1+0.5*beta))) )};
        double lambda {(1-std::sqrt(dynamo(beta,cosa))-0.5*beta)/(1+std::sqrt(dynamo(beta,cosa))+0.5*beta)};
        return lambda*lambda/( dispersion*(1./(cosa*cosa)+(lambda*lambda-1)) );
    }
}

double Brnd_mhd::hf (const double &beta,
                     const double &cosa) const{
    if (cosa<1e-6){
        return 0;
    }
    else {
        double dispersion {0.5*(1+0.5*beta)*( 1+std::sqrt(1-2*beta*cosa*cosa/((1+0.5*beta)*(1+0.5*beta))) )};
        double lambda {(1-std::sqrt(dynamo(beta,cosa))+0.5*beta)/(1+std::sqrt(dynamo(beta,cosa))-0.5*beta)};
        return 1./(dispersion*(1+lambda*lambda*(1./(cosa*cosa) -1)));
    }
}

double Brnd_mhd::speca (const double &k,
                        const Param *par) const{
    //units fixing, wave vector in 1/kpc units
    const double p0 {par->brnd_mhd.pa0};
    const double kr {k/par->brnd_mhd.k0};
    const double a0 {par->brnd_mhd.aa0};
    const double unit = 1./(4*CGS_U_pi*k*k);
    // power law
    double P {0.};
    if (kr>=1.){
        P = p0/std::pow(kr,a0);
    }
    return P*unit;
}

double Brnd_mhd::specf (const double &k,
                        const Param *par) const{
    //units fixing, wave vector in 1/kpc units
    const double p0 {par->brnd_mhd.pf0};
    const double kr {k/par->brnd_mhd.k0};
    const double a0 {par->brnd_mhd.af0};
    const double unit = 1./(4*CGS_U_pi*k*k);
    // power law
    double P{0.};
    if (kr>=1.){
        P = p0/std::pow(kr,a0);
    }
    return P*unit;
}

double Brnd_mhd::specs (const double &k,
                        const Param *par) const{
    //units fixing, wave vector in 1/kpc units
    const double p0 {par->brnd_mhd.ps0};
    const double kr {k/par->brnd_mhd.k0};
    const double a0 {par->brnd_mhd.as0};
    const double unit = 1./(4*CGS_U_pi*k*k);
    // power law
    double P {0.};
    if (kr>=1.){
        P = p0/std::pow(kr,a0);
    }
    return P*unit;
}
// END