# Integrators

![type:video](https://www.youtube.com/embed/QW5a-iH62dQ)

Numerical integrators are the backbone of any N-body package. 
A numerical integrator evolves particles forward in time, one timestep at a time.
To do that, the integrator needs to know the current position and velocity coordinates of the particles, and the equations of motion which come in the form of a set of ordinary differential equations.

Because an exact solution to these differential equations is in general unknown, each integrator attempts to approximate the true solution numerically. 
Different integrators do this differently and each of them has some advantages and some disadvantages. 
Each of the built-in integrators of REBOUND is described in this section.

## IAS15

![type:video](https://www.youtube.com/embed/UILEgdZt-fw)

IAS15 stands for **I**ntegrator with **A**daptive **S**tep-size control, **15**th order. It is a very high order, non-symplectic integrator which can handle arbitrary forces (including those who are velocity dependent). 
It is in most cases accurate down to machine precision (16 significant decimal digits). 
The IAS15 implementation in REBOUND can integrate variational equations. 
The algorithm is described in detail in [Rein & Spiegel 2015](https://ui.adsabs.harvard.edu/abs/2015MNRAS.446.1424R/abstract) and also in the original paper by [Everhart 1985](https://ui.adsabs.harvard.edu/abs/1985ASSL..115..185E/abstract). 


IAS15 is the default integrator of REBOUND, so if you want to use it, you don't need to do anything. 
However, you can also set it explicitly:

=== "C"
    ```c
    struct reb_simulation* r = reb_create_simulation();
    r->integrator = REB_INTEGRATOR_IAS15;
    ```

=== "Python"
    ```python
    sim = rebound.Simulation()
    sim.integrator = "ias15"
    ```

The setting for IAS15 are stored in the `reb_simulation_integrator_ias15` structure. 

`epsilon` (`double`)
:   IAS15 is an adaptive integrator. It chooses its timesteps automatically. This parameter controls the accuracy of the integrator. The default value is $10^{-9}$. Setting this parameter to 0 turns off adaptive timestepping and a constant timestep will is used. Turning off adaptive time-stepping is rarely useful. 

    !!! Important
        It is tempting to change `epsilon` to achieve a speedup at the loss of some accuracy. However, that makes rarely sense. The reason is that IAS15 is a very high (15th!) order integrator. Suppose we increase the timestep by a factor of 10. This will increase the error by a factor of $10^{15}$. In other words, a simulation that previously was converged to machine precision will now have an error of order unity. 

`min_dt` (`double`)
:   This sets the minimum allowed timestep. The default value is 0. Set this to a finite value if the adaptive timestep becomes excessively small, for example during close encounters or because of finite floating point precision. Use with caution and make sure the simulation results still make physically sense as you might be in danger of ignoring small timescales in the problem. 
    The following code sets the smallest timestep to $10^{-3}$ time units: 
    === "C"
        ```c
        struct reb_simulation* r = reb_create_simulation();
        r->ri_ias15.min_dt = 1e-3;
        ```

    === "Python"
        ```python
        sim = rebound.Simulation()
        sim.ri_ias15.min_dt = 1e-3
        ```

`epsilon_global` `(unsigned int`)
:   This flag determines how the relative acceleration error is estimated. If set to 1, IAS15 estimates the fractional error via `max(acceleration_error)/max(acceleration)` where the maximum is taken over all particles. If set to 0, the fractional error is estimates via `max(acceleration_error/acceleration)`.

All other members of this structure are only for internal IAS15 use.


## WHFast

![type:video](https://www.youtube.com/embed/ttLUhtNj1Lc)

WHFast is an implementation of the symplectic [Wisdom-Holman](https://ui.adsabs.harvard.edu/abs/1991AJ....102.1528W/abstract) integrator. 
It is the best choice for systems in which there is a dominant central object and perturbations to the Keplerian orbits are small. 
It supports first and second symplectic correctors as well as the kernel method of [Wisdom et al. 1996](https://ui.adsabs.harvard.edu/abs/1996FIC....10..217W/abstract) with various different kernels.
The basic implementation of WHFast is described in detail in [Rein & Tamayo 2015](https://ui.adsabs.harvard.edu/abs/2015MNRAS.452..376R/abstract). 
The higher order aspects of it are described in [Rein, Tamayo & Brown 2019](https://ui.adsabs.harvard.edu/abs/2019MNRAS.489.4632R/abstract). 
WHFast also supports first order variational equations which can be used in chaos estimators ([Rein & Tamayo 2016](https://ui.adsabs.harvard.edu/abs/2016MNRAS.459.2275R/abstract)). 
The user can choose between Jacobi and Democratic Heliocentric coordinates. 

The following code enables the WHFast integrator. 
Because WHFast is not an adaptive integrator, you also need to set a timestep.
Typically, this should be a small fraction (a few percent) of the smallest dynamical timescale in the problem.
=== "C"
    ```c
    struct reb_simulation* r = reb_create_simulation();
    r->integrator = REB_INTEGRATOR_WHFAST;
    r->dt = 0.1; 
    ```

=== "Python"
    ```python
    sim = rebound.Simulation()
    sim.integrator = "whfast"
    sim.dt = 0.1
    ```


The setting for WHFast are stored in the `reb_simulation_integrator_whfast` structure, which itself is part of the simulation structure. 

`unsigned int corrector`
:   This variable turns on/off different first symplectic correctors for WHFast. 
    By default, it is set to zero and symplectic correctors are turned off. 

    First symplectic correctors remove error terms up to $O(\epsilon \cdot dt^p)$, where $p$ is the order of the symplectic corrector, and $\epsilon$ is the mass ratio in the system.
    The following first correctors are implemented in REBOUND:

    Order   | Number of stages 
    ------- | ----------------
    0       | Correctors turned off (default)
    3       | 2
    5       | 4 
    7       | 6
    11      | 10  
    17      | 16

    For most cases you want to choose the 17th order corrector. 
    You only want to consider lower order correctors if frequent outputs are required and speed is an issue.
    Symplectic correctors are turned on as follows.

    
    === "C"
        ```c
        r->ri_whfast.corrector = 17;
        r->ri_whfast.safe_mode = 0;
        ```

    === "Python"
        ```python
        sim.ri_whfast.corrector = 17
        sim.ri_whfast.safe_mode = 0
        ```
    
    Note that the above code also turns off the safe mode. 
    Most likely, you want to do that too (see below for a description of the safe mode).

`unsigned int corrector2`
:   This variable turns on/off second symplectic correctors for WHFast. 
    By default, second symplectic correctors are off (0). 
    Set to 1 to use second symplectic correctors.

    !!! Info
        The nomenclature can be a bit confusing. 
        First symplectic correctors are different from second symplectic correctors.
        And in REBOUND first symplectic correctors have different orders (see above). 
        Second symplectic correctors on the other hand can only be turned on or off. 
        See [Rein, Tamayo & Brown 2019](https://ui.adsabs.harvard.edu/abs/2019MNRAS.489.4632R/abstract) for more on high order symplectic integrators.

`unsigned int kernel`
:   This variable determines the kernel of the WHFast integrator. 
    The following options are currently supported:

    - The standard Wisdom-Holman kick step. This is the default.
    - Exact modified kick. This works for Newtonian gravity only. Not additional forces. 
    - The composition kernel.
    - Lazy implementer's modified kick. This is often the best option.
        
    Check [Rein, Tamayo & Brown 2019](https://ui.adsabs.harvard.edu/abs/2019MNRAS.489.4632R/abstract) for details on what these kernel methods are. 
    The syntax to use them is 
    
    === "C"
        ```c
        r->ri_whfast.kernel = REB_WHFAST_KERNEL_DEFAULT;      // or
        r->ri_whfast.kernel = REB_WHFAST_KERNEL_MODIFIEDKICK; // or
        r->ri_whfast.kernel = REB_WHFAST_KERNEL_COMPOSITION;  // or
        r->ri_whfast.kernel = REB_WHFAST_KERNEL_LAZY;
        ```

    === "Python"
        ```python
        sim.ri_whfast.kernel = "default"      # or
        sim.ri_whfast.kernel = "modifiedkick" # or
        sim.ri_whfast.kernel = "composition"  # or
        sim.ri_whfast.kernel = "lazy"
        ```

`unsigned int coordinates`
:   WHFast supports different coordinate systems. 
    Default are Jacobi Coordinates.
    Other options are democratic heliocentric coordinates, and the WHDS coordinates ([Hernandez & Dehnen, 2017](https://ui.adsabs.harvard.edu/abs/2017MNRAS.468.2614H/abstract))
    The syntax to use them is 
    
    === "C"
        ```c
        r->ri_whfast.coordinates = REB_WHFAST_COORDINATES_JACOBI;                  // or
        r->ri_whfast.coordinates = REB_WHFAST_COORDINATES_DEMOCRATICHELIOCENTRIC;  // or
        r->ri_whfast.coordinates = REB_WHFAST_COORDINATES_WHDS; 
        ```

    === "Python"
        ```python
        sim.ri_whfast.coordinates = "jacobi"                 # or
        sim.ri_whfast.coordinates = "democraticheliocentric" # or
        sim.ri_whfast.coordinates = "whds"
        ```

`unsigned int recalculate_coordinates_this_timestep`
:   Setting this flag to one will recalculate the internal coordinates from the particle structure in the next timestep. 
    After the timestep, the flag gets set back to 0. If you want to change particles after every timestep, you also need to set this flag to 1 before every timestep. Default is 0.

`unsigned int safe_mode`
:   If this flag is set (the default), WHFast will recalculate the internal coordinates (Jacobi/heliocentric/WHDS) and synchronize every timestep, to avoid problems with outputs or particle modifications between timesteps. 
    Setting it to 0 will result in a speedup, but care must be taken to synchronize and recalculate the internal coordinates when needed. See also the AdvWHFast.ipynb tutorial.

`unsigned int keep_unsynchronized`
:   This flag determines if the inertial coordinates generated are discarded in subsequent timesteps (cached Jacobi/heliocentric/WHDS coordinates are used instead). The default is 0. Set this flag to 1 if you require outputs and bit-wise reproducibility

All other members of the `reb_simulation_integrator_whfast` structure are for internal use only.

## Gragg-Bulirsch-Stoer (BS)
The Gragg-Bulirsch-Stoer integrator (short BS for Bulirsch-Stoer) is an adaptive integrator which uses Richardson extrapolation and the modified midpoint method to obtain solutions to ordinary differential equations.

The version in REBOUND is based on the method described in Hairer, Norsett, and Wanner 1993 (see section II.9, page 224ff), specifically the JAVA implementation available in the [Hipparchus package](https://github.com/Hipparchus-Math/hipparchus/blob/master/hipparchus-ode/src/main/java/org/hipparchus/ode/nonstiff/GraggBulirschStoerIntegrator.java). The Hipparchus as well as the REBOUND version are adaptive in both the timestep and the order of the method for optimal performance. 
The BS implementation in REBOUND can integrate first and second orer variational equations. 

The BS integrator is particularly useful for short integrations where only medium accuracy is required. For long integrations a symplectic integrator such as WHFast performs better. For high accuracy integrations the IAS15 integrator performs better. Because BS is adaptive, it can handle close encounters. Currently a collision search is only performed after every timestep, i.e. not after a sub-timestep.

The following code enables the BS integrator and sets both the relative and absolute tolerances to 0.0001 (the default is $10^{-8}$):

=== "C"
    ```c
    struct reb_simulation* r = reb_create_simulation();
    r->integrator = REB_INTEGRATOR_BS;
    r->ri_bs.eps_rel = 1e-4;
    r->ri_bs.eps_abs = 1e-4;
    ```

=== "Python"
    ```python
    sim = rebound.Simulation()
    sim.integrator = "BS"
    sim.ri_bs.eps_rel = 1e-4
    sim.ri_bs.eps_abs = 1e-4
    ```

The BS integrator tries to keep the error of each coordinate $y$ below $\epsilon_{abs} + \epsilon_{rel} \cdot  \left|y\right|$. Note that this applies to both position and velocity coordinates of all particles which implues that the code units you're choosing for the integration matter. If you need fine control over the scales used internally, you can set the `getscale` function pointer in `r->ri_bs.nbody_ode` (this is currently undocumented, search the source code for `getscale` to find out more).

!!! Info
        The code does not guarantee that the errors remain below the tolerances. In particular, note that BS is not a symplectic integrator which results in errors growing linearly in time (phase errors grow quadratically in time). It requires some experimentation to find the tolerances that offer the best compromise between accuracy and speed for your specific problem. 


You can limit the timestep with both a maximum and minimum timestep:

=== "C"
    ```c
    r->ri_bs.min_dt = 1e-5;
    r->ri_bs.max_dt = 1e-2;
    ```

=== "Python"
    ```python
    sim.ri_bs.min_dt = 1e-5
    sim.ri_bs.max_dt = 1e-2
    ```

Compared to the other integrators in REBOUND, BS can be used to integrate arbitrary ordinary differential equations (ODEs), not just the N-body problem. We expose an ODE-API in REBOUND which allows you to make use of this. User-defined ODEs are always integrated with BS. You can choose to integrate the N-body equations with BS as well, or any of the other integrators. 

If you choose BS for the N-body equations, then BS will treat all ODEs (N-body + all user-defined ones) as one big system of coupled ODEs. This means your timestep will be set by either the N-body problem or the user-defined ODEs, whichever involves the shorter timescale.

If you choose IAS15 or WHFast for the N-body equation but also have user-defined ODEs, then they cannot be treated as one big coupled system of ODEs anymore. In that case the N-body integration is done first. Then the user-defined ODEs are advanced to the exact same time as the N-body system using BS using whatever timestep is required to achieve the tolerance set in the `ri_bs` struct. During the integration of the user-defined ODEs, the coordinates of the particles in the N-body simulation are assumed to be fixed at their final position and velocity. This introduces an error. However, if the system evolves adiabatically (the timescales in the user-defined ODEs are much longer than in the N-body problem), then the error will be small. 

The following code sets up a REBOUND simulation in which a harmonic oscillator is driven by the phase of a planet orbiting a star:

=== "C"
    ```c
    void derivatives(struct reb_ode* const ode, double* const yDot, const double* const y, const double t){
        struct reb_orbit o = reb_tools_particle_to_orbit(ode->r->G, ode->r->particles[1], ode->r->particles[0]);
        const double omega = 1;
        double forcing = sin(o.f);
        yDot[0] = y[1];
        yDot[1] = -omega*omega*y[0] + forcing;
    }

    void run(){
        struct reb_simulation* r = reb_create_simulation();
        reb_add_fmt(r, "m", 1.);
        reb_add_fmt(r, "m a e", 1e-3, 1., 0.1);

        r->integrator = REB_INTEGRATOR_BS;

        struct reb_ode* ho = reb_create_ode(r,2);   // Add an ODE with 2 dimensions
        ho->derivatives = derivatives;              // Right hand side of the ODE
        ho->y[0] = 1;                               // Initial conditions
        ho->y[1] = 0;
    }
    ```

=== "Python"
    ```python
    import numpy as np

    def derivatives(ode, yDot, y, t):
        omega = 1.0
        sim_pointer = ode.contents.r
        orbit = sim_pointer.contents.particles[1]
        forcing = np.sin(orbit.f)
        yDot[0] = y[1]
        yDot[1] = -omega*omega*y[0] + forcing

    sim = rebound.Simulation()
    sim.add(m=1)
    sim.add(m=1e-3, a=1, e=0.1)

    sim.integrator = "BS"

    ho = sim.create_ode(length=2)   # Add an ODE with 2 dimensions
    ho.derivatives = derivatives    # Right hand side of the ODE
    ho.y[0] = 1.0                   # Initial conditions
    ho.y[1] = 0.0
    ```


## Mercurius

MERCURIUS is a hybrid symplectic integrator very similar to MERCURY ([Chambers 1999](https://ui.adsabs.harvard.edu/abs/1999MNRAS.304..793C/abstract)). 
It uses WHFast for long term integrations but switches over smoothly to IAS15 for close encounters.  
The MERCURIUS implementation is described in [Rein et al 2019](https://ui.adsabs.harvard.edu/abs/2019MNRAS.485.5490R/abstract).

    
The following code enables MERCURIUS and sets the critical radius to 4 Hill radii
=== "C"
    ```c
    struct reb_simulation* r = reb_create_simulation();
    r->integrator = REB_INTEGRATOR_MERCURIUS;
    r->ri_mercurius.hillfac = 4.;
    ```

=== "Python"
    ```python
    sim = rebound.Simulation()
    sim.integrator = "mercurius"
    sim.ri_mercurius.hillfac = 4.
    ```

The `reb_simulation_integrator_mercurius` structure contains the configuration and data structures used by the hybrid symplectic MERCURIUS integrator.

`double (*L) (const struct reb_simulation* const r, double d, double dcrit)`
:   This is a function pointer to the force switching function. 
    If NULL (the default), the MERCURY switching function will be used. 
    The argument `d` is the distance between two particles. 
    The argument `dcrit` is the maximum critical distances of the two particles. 
    The return value is a scalar between 0 and 1. 
    If this function always returns 1, then the integrator effectively becomes the standard Wisdom-Holman integrator.

    The following switching functions are available:


    - Mercury switching function 

        This is the same polynomial switching function as used in MERCURY. 

        ```c
        double reb_integrator_mercurius_L_mercury(const struct reb_simulation* const r, double d, double dcrit);           
        ```
    - Smooth switching functions 

        These two polynomials switching functions are 4 and 5 times differentiable. 
        Using smooth switching functions can improve the accuracy. 
        For a detailed discussion see [Hernandez 2019](https://ui.adsabs.harvard.edu/abs/2019MNRAS.490.4175H/abstract). 

        ```c
        double reb_integrator_mercurius_L_C4(const struct reb_simulation* const r, double d, double dcrit);
        double reb_integrator_mercurius_L_C5(const struct reb_simulation* const r, double d, double dcrit); 
        ```

    - Infinitely differentiable switching function

        This is an infinitely differentiable switching function. 

        ```c
        double reb_integrator_mercurius_L_infinity(const struct reb_simulation* const r, double d, double dcrit); 
        ```
   
    The switching function can be set using this syntax: 

    === "C"
        ```c
        struct reb_simulation* r = reb_create_simulation();
        r->ri_mercurius.L = reb_integrator_mercurius_L_infinity; 
        ```

    === "Python"
        ```python
        sim = rebound.Simulation()
        sim.ri_mercurius.L = "infinity"
        ```

`double hillfac`
:   The critical switchover radii of particles are calculated automatically based on multiple criteria. One criterion calculates the Hill radius of particles and then multiplies it with the `hillfac` parameter. The parameter is in units of the Hill radius. The default value is 3. 

`unsigned int recalculate_coordinates_this_timestep`
:   Setting this flag to one will recalculate heliocentric coordinates from the particle structure at the beginning of the next timestep. After a single timestep, the flag gets set back to 0. If one changes a particle manually after a timestep, then one needs to set this flag to 1 before the next timestep.

`unsigned int recalculate_dcrit_this_timestep`
:   Setting this flag to one will recalculate the critical switchover distances dcrit at the beginning of the next timestep. After one timestep, the flag gets set back to 0. If you want to recalculate `dcrit` at every timestep, you also need to set this flag to 1 before every timestep.

`unsigned int safe_mode`
:   If this flag is set to 1 (the default), the integrator will recalculate heliocentric coordinates and synchronize after every timestep to avoid problems with outputs or particle modifications between timesteps. Setting this flag to 0 will result in a speedup, but care must be taken to synchronize and recalculate coordinates manually if needed.




## SABA

SABA are symplectic integrators developed by [Laskar & Robutel 2001](https://ui.adsabs.harvard.edu/abs/2001CeMDA..80...39L/abstract) and [Blanes et al. 2013](https://ui.adsabs.harvard.edu/abs/2012arXiv1208.0689B/abstract). 
The implementation in REBOUND supports SABA1, SABA2, SABA3, and SABA4 as well as the corrected versions SABAC1, SABAC2, SABAC3, and SABAC4. 
Different correctors can be selected. 
In addition, the following methods with various generalized orders are supported: SABA(8,4,4), SABA(8,6,4), SABA(10,6,4). 
See [Rein, Tamayo & Brown 2019](https://ui.adsabs.harvard.edu/abs/2019MNRAS.489.4632R/abstract) for details on how these methods work.

The `reb_simulation_integrator_saba` structure contains the configuration and data structures used by the SABA integrator family.

`unsigned int type`
:   This parameter specifies which SABA integrator type is used.
    The following SABA integrators are supported:

    Numerical value     |  C constant name    | Description 
    ------------------- | ------------------- | ----------------------------------
    0x0                 | `REB_SABA_1`        | SABA1 (Wisdom-Holman)
    0x1                 | `REB_SABA_2`        | SABA2
    0x2                 | `REB_SABA_3`        | SABA3
    0x3                 | `REB_SABA_4`        | SABA4
    0x100               | `REB_SABA_CM_1`     | SABACM1 (Modified kick corrector)
    0x101               | `REB_SABA_CM_2`     | SABACM2 (Modified kick corrector)
    0x102               | `REB_SABA_CM_3`     | SABACM3 (Modified kick corrector)
    0x103               | `REB_SABA_CM_4`     | SABACM4 (Modified kick corrector)
    0x200               | `REB_SABA_CL_1`     | SABACL1 (lazy corrector)
    0x201               | `REB_SABA_CL_2`     | SABACL2 (lazy corrector)
    0x202               | `REB_SABA_CL_3`     | SABACL3 (lazy corrector)
    0x203               | `REB_SABA_CL_4`     | SABACL4 (lazy corrector)
    0x4                 | `REB_SABA_10_4`     | SABA(10,4), 7 stages
    0x5                 | `REB_SABA_8_6_4`    | SABA(8,6,4), 7 stages
    0x6                 | `REB_SABA_10_6_4`   | SABA(10,6,4), 8 stages, default
    0x7                 | `REB_SABA_H_8_4_4`  | SABAH(8,4,4), 6 stages
    0x8                 | `REB_SABA_H_8_6_4`  | SABAH(8,6,4), 8 stages
    0x9                 | `REB_SABA_H_10_6_4` | SABAH(10,6,4), 9 stages

    SABA(10,6,4) is the default integrator. It has a generalized order of $O(\epsilon dt^{10} + \epsilon^2 dt^6 + \epsilon^3 dt^4)$. 

    Below is an example on how to enable the SABA integrators in REBOUND and set a specific type.

    === "C"
        ```c
        struct reb_simulation* r = reb_create_simulation();
        r->integrator = REB_INTEGRATOR_SABA;
        r->ri_saba.type = REB_SABA_10_6_4;
        ```

    === "Python"
        ```python
        sim = rebound.Simulation()
        sim.integrator = "saba"
        sim.ri_saba.type = "(10,6,4)"
        ```
        One can also use the following shorthand: 
        ```python
        sim = rebound.Simulation()
        sim.integrator = "SABA(10,6,4)"
        ```

`unsigned int safe_mode`
:   This flag has the same functionality as in WHFast. Default is 1. Setting this to 0 will provide a speedup, but care must be taken with synchronizing integration steps and modifying particles.

`unsigned int keep_unsynchronized`
:   This flag determines if the inertial coordinates generated are discarded in subsequent timesteps (cached Jacobi coordinates are used instead). The default is 0. Set this flag to 1 if you require outputs and bit-wise reproducibility 




## JANUS
Janus is a bit-wise time-reversible high-order symplectic integrator using a mix of floating point and integer arithmetic.
It is described in [Rein & Tamayo 2018](https://ui.adsabs.harvard.edu/abs/2018MNRAS.473.3351R/abstract).

The following code shows how to enable JANUS and set the length and velocity scales.
=== "C"
    ```c
    struct reb_simulation* r = reb_create_simulation();
    r->integrator = REB_INTEGRATOR_JANUS;
    r->ri_janus.scale_pos = 1e-10;
    r->ri_janus.scale_vel = 1e-10;
    ```

=== "Python"
    ```python
    sim = rebound.Simulation()
    sim.integrator = "janus"
    sim.ri_janus.scale_pos = 1e-10
    sim.ri_janus.scale_vel = 1e-10
    ```


The `reb_simulation_integrator_janus` structure contains the configuration and data structures used by the bib-wise reversible JANUS integrator.

`double scale_pos`
:   Scale of the problem. Positions get divided by this number before the conversion to an integer. Default: $10^{-16}$.

`double scale_vel`
:   Scale of the problem. Velocities get divided by this number before the conversion to an integer. Default: $10^{-16}$. 

`unsigned int order`
:   The order of the scheme. Default is 6.

`unsigned int recalculate_integer_coordinates_this_timestep`
:   If this flag is set, then JANUS will recalculate the integer coordinates from floating point coordinates at the next timestep.

All other members of this structure are only for internal use and should not be changed manually.


## Embedded Operator Splitting Method (EOS)
This is the Embedded Operator Splitting (EOS) methods described in [Rein 2019](https://ui.adsabs.harvard.edu/abs/2020MNRAS.492.5413R/abstract).

The `reb_simulation_integrator_eos` structure contains the configuration and data structures used by EOS.

`unsigned int phi0`
:   Outer operator splitting scheme (see below for options)

`unsigned int phi1`
:   Inner operator splitting scheme (see below for options)

`unsigned int n`
:   Number of sub-timesteps. Default: 2. 

`unsigned int safe_mode`
:   If set to 0, always combine drift steps at the beginning and end of `phi0`. If set to 1, `n` needs to be bigger than 1.



The following operator splitting methods for `phi0` and `phi1` are supported in the EOS integrator.

Numerical value | Constant name         | Description
--------------- | --------------------- | -------------------------------------------------
0x00            | `REB_EOS_LF`          | 2nd order, standard leap-frog
0x01            | `REB_EOS_LF4`         | 4th order, three function evaluations
0x02            | `REB_EOS_LF6`         | 6th order, nine function evaluations
0x03            | `REB_EOS_LF8`         | 8th order, seventeen function evaluations, see Blanes & Casa (2016), p91
0x04            | `REB_EOS_LF4_2`       | generalized order (4,2), two force evaluations, McLachlan 1995
0x05            | `REB_EOS_LF8_6_4`     | generalized order (8,6,4), seven force evaluations
0x06            | `REB_EOS_PLF7_6_4`    | generalized order (7,6,4), three force evaluations, pre- and post-processors
0x07            | `REB_EOS_PMLF4`       | 4th order, one modified force evaluation, pre- and post-processors, Blanes et al. (1999)
0x08            | `REB_EOS_PMLF6`       | 6th order, three modified force evaluations, pre- and post-processors, Blanes et al. (1999)


The following code shows how to enable EOS and set the embedded methods.
=== "C"
    ```c
    struct reb_simulation* r = reb_create_simulation();
    r->integrator = REB_INTEGRATOR_EOS;
    r->ri_eos.phi0 = REB_EOS_LF4;
    r->ri_eos.phi1 = REB_EOS_LF4;
    r->ri_eos.n = 6;
    ```

=== "Python"
    ```python
    sim = rebound.Simulation()
    sim.integrator = "eos"
    sim.ri_eos.phi0 = "LF4"
    sim.ri_eos.phi1 = "LF4"
    sim.ri_eos.n = 6
    ```

## Leapfrog
`REB_INTEGRATOR_LEAPFROG`     

This is the standard leap frog integrator. It is second order and symplectic. No configuration is available (the timestep is set in the simulation structure).

## Symplectic Epicycle Integrator (SEI)
`REB_INTEGRATOR_SEI`          

Symplectic Epicycle Integrator (SEI), mixed variable symplectic integrator for the shearing sheet, second order, Rein & Tremaine 2011. The `reb_simulation_integrator_sei` structure contains the configuration and data structures used by the Symplectic Epicycle Integrator (SEI).

`double OMEGA`
:   Epicyclic/orbital frequency. This can be set as follows:
    === "C"
        ```c
        struct reb_simulation* r = reb_create_simulation();
        r->integrator = REB_INTEGRATOR_SEI;
        r->ri_sei.OMEGA = 1.0;
        ```

    === "Python"
        ```python
        sim = rebound.Simulation()
        sim.integrator = "sei"
        sim.ri_sei.OMEGA = 1.0
        ```

`double OMEGAZ`
:   Epicyclic frequency in vertical direction. Defaults to `OMEGA` if not set.

All other members of this structure are only for internal use and should not be changed manually.


## TES

TES stands for **T**errestrial **E**xoplanet **S**imulator. TES builds upon the classic Encke method and integrates only the perturbations to Keplerian trajectories to reduce both the error and runtime of simulations. Variable step size is used throughout to enable close encounters to be precisely handled.
The algorithm is described in detail in [Bartram & Wittig 2021](https://ui.adsabs.harvard.edu/abs/2021MNRAS.504..678B/abstract). 
    
!!! Important
    TES is a new addition to REBOUND. Whereas it has been tested extensively, you might experience some bugs and there are likely edge cases where it will not return physical results. It is therefore especially important to make sure that simulations using TES are convergend and not dependent on any numerical parameters. This can be done by varying the timestep and other paramters or by comparing results to simulations using other integrators. Please report any issue that you encounter on GitHub.



The following code shows how to enable TES and how to set some of its control parameters. 

=== "C"
    ```c
    struct reb_simulation* r = reb_create_simulation();
    r->integrator = REB_INTEGRATOR_TES;
    r->ri_tes.dq_max = 1e-2;
    r->ri_tes.recti_per_orbit = 1.61803398875;
    r->ri_tes.epsilon = 1e-6;
    ```

=== "Python"
    ```python
    sim = rebound.Simulation()
    sim.integrator = "tes"
    sim.ri_tes.dq_max = 1e-2
    sim.ri_tes.recti_per_orbit = 1.61803398875
    sim.ri_tes.epsilon = 1e-6
    ```

The setting for TES are stored in the `reb_simulation_integrator_tes` structure. For almost all use cases TES will work best with default settings for all configuration variables below and can therefore be left uninitialised in your code. The two cases where a user may want the adjust these values are if: 1) a severe shrinkage of the semi-major axis of the inner-most planet is expected; 2) the ratio of the most massive planet mass to that of the star exceeds 1e-2, and in this case it is typically better to use IAS15.

`dq_max` (`double`)
:   The value of dq/q that triggers a rectification. Generally, this variable can be left at the default value of $10^{-2}$ as the `recti_per_orbit` variable is the main rectification trigger. One exception is for systems where the ratio of the most massive planet's mass to the stellar mass is greater than $10^{-2}$, and in this case the value should be set to that ratio to avoid excessive rectification frequency. However, in this use case it is typically better to use IAS15 instead.    

`recti_per_orbit` (`double`)
:   The number of rectifications per orbit. This variable can be brought closer to 1 to slightly reduce computational costs in low mass ratio (planets to star) systems but the default value of 1.61803398875, the golden ratio, is recommended to ensure accuracy. 

`epsilon` (`double`)
:   TES is an adaptive integrator. It chooses its timesteps automatically. This parameter controls the accuracy of the integrator. The default value is $10^{-6}$ which has been chosen heuristically from a wide range of test cases and is therefore the recommended value for all integrations.

    !!! Important
        It is tempting to change `epsilon` to achieve a speedup at the loss of some accuracy. However, that makes rarely sense. The reason is that TES is a very high (15th!) order integrator. Suppose we increase the timestep by a factor of 10. This will increase the error by a factor of $10^{15}$. In other words, a simulation that previously was converged to machine precision will now have an error of order unity. 
        
`orbital_period` `(double`)
:   This specifies the shortest dynamic period of all objects in the system and is set automatically by REBOUND. The only use case for changing this is if you are expecting severe shrinkage of the semi-major axis of the inner-most planet and in this case it might make sense to reduce the period accordingly.

`warnings` `(uint32_t`)
:   To silence the TES warning message, set this variable to 1. 


All other members of this structure are only for internal TES use.


## No integrator
Sometimes it might make sense to simply not advance any particle positions or velocities. By selecting this integrator, one can still perform integration steps, but particles will not move.

Here is how to do that:
=== "C"
    ```c
    struct reb_simulation* r = reb_create_simulation();
    r->integrator = REB_INTEGRATOR_NONE;
    ```

=== "Python"
    ```python
    sim = rebound.Simulation()
    sim.integrator = "none"
    ```
