@echo off

git submodule update --init --recursive

@REM set build_type=Debug
set build_type=Release
@REM set build_type=RelWithDebInfo
set is_build_test=OFF
set is_install=ON
set user_install_prefix=install
echo build type [%build_type%]
echo build test switch [%is_build_test%]
echo install switch [%is_install%]
echo install prefix [%user_install_prefix%]

rd /s /q build-vs2019-x64
md build-vs2019-x64
cd build-vs2019-x64

cmake .. ^
-G "Visual Studio 16 2019" ^
-A x64 ^
-DIS_BUILD_TEST=%is_build_test% ^
-DIS_BUILD_INSTALL=%is_install% ^
-DUSER_INSTALL_PREFIX=%user_install_prefix% 

if %is_install%==OFF (
    cmake --build ./ --config %build_type% --target ALL_BUILD -j 6
) else (
    cmake --build ./ --config %build_type% --target ALL_BUILD install -j 6
)

pause