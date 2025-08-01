idf_component_register(
    SRCS
        # 核心数学
        src/gen.cc
        src/vecteur.cc
        src/global.cc
        src/input_lexer.cc
        src/input_parser.cc
        src/gausspol.cc
        src/sym2poly.cc
        src/derive.cc
        src/solve.cc
        src/intg.cc
        src/intgab.cc
        src/risch.cc
        src/series.cc
        src/lin.cc
        src/alg_ext.cc
        src/desolve.cc
        src/csturm.cc
        src/ezgcd.cc
        src/modfactor.cc
        src/modpoly.cc
        src/Equation.cc

        # 符号 / 脚本
        src/symbolic.cc
        src/prog.cc
        src/rpn.cc
        # src/cas2.cc
        # src/casctrl.cc
        src/maple.cc
        src/ti89.cc
        src/mathml.cc
        src/tex.cc
        src/help.cc
        src/Help1.cc
        src/misc.cc
        src/permu.cc
        src/isom.cc
        src/gauss.cc
        src/quater.cc

        # 可选：绘图 / 3D（想省就删）
        src/plot.cc
        src/plot3d.cc
        src/Graph.cc
        src/graphe.cc
        src/graphtheory.cc

        # 缺失的通用工具
        src/index.cc
        src/Print.cc
        src/cocoa.cc
        src/pari.cc
        src/unary.cc
        src/usual.cc
        src/identificateur.cc
        src/first.cc
        src/tinymt32.cc
        src/ifactor.cc

        src/subst.cc
        src/sparse.cc
        src/threaded.cc
        src/moyal.cc

        src/sha256.c

    INCLUDE_DIRS
        "src"
    REQUIRES
        newlib tommath
    LDFRAGMENTS 
        "giac_extram.lf"
)
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    IN_GIAC=1 HAVE_CONFIG_H=1 USE_GMP_REPLACEMENTS=1
)
target_compile_options(${COMPONENT_LIB} PUBLIC 
    -w
    -Wno-misleading-indentation
    -Wno-unknown-pragmas
    -Wno-maybe-uninitialized
    -Wno-unused-variable
    -Wno-unused-value
    -Wno-uninitialized
    -Wno-char-subscripts
    -Wno-array-bounds
    -Wno-catch-value
    -Wno-narrowing
    -Wno-address
    -Wno-parentheses
    -Wno-comment
    -Wno-reorder
    -mtext-section-literals
)
target_link_options(${COMPONENT_LIB} PUBLIC
    -Wl,--gc-sections
    -Wl,--relax
)
project(giac)
