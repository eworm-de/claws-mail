#!/bin/bash

PERL_SCRIPT="sylpheed-kdeservicemenu.pl"
DESKTOP_TEMPLATE_ONE="template_sylpheed-attach-files.desktop"
DESKTOP_ONE="sylpheed-attach-files.desktop"
DESKTOP_TEMPLATE_TWO="template_sylpheed-compress-attach.desktop"
DESKTOP_TWO="sylpheed-compress-attach.desktop"

function check_environ {
#Check to see if we can coax kde-config into the PATH
echo "Checking for kde-config..."
if [ -z "$(type 'kde-config' 2> /dev/null)" ]; then #Odd way of checking if kde-config is in $PATH
  echo "kde-config not found, checking for \$KDEDIR to compensate..."
  if [ ! -z $KDEDIR ]; then
    export PATH=$PATH:$KDEDIR/bin
  else
    echo "***"
    echo "***$0 cannot figure out where KDE is installed."
    echo "***kde-config is not in \$PATH, and \$KDEDIR is not set."
    echo "***To fix this, manually change \$PATH to add the KDE executables."
    echo "***E.g. export PATH=\$PATH:/opt/kde/bin"
    echo "***It would also be a good idea to add this line to your shell login/profile."
    echo "***"
    echo
    echo "Nothing was installed or removed."
    exit 1
  fi
fi
echo "Okay."
}

function install_all {
#Go ahead and install
echo "Generating $DESKTOP_ONE ..."
SED_PREFIX=${PREFIX//\//\\\/} #Replace forward slashes in $PREFIX with \/ so that sed doesn't freak out
sed "s/SCRIPT_PATH/$SED_PREFIX\\/bin\\/sylpheed-kdeservicemenu.pl/" $DESKTOP_TEMPLATE_ONE > $DESKTOP_ONE
cp -f $DESKTOP_ONE $PREFIX/share/apps/konqueror/servicemenus/sylpheed-attach-files.desktop
echo "Generating $DESKTOP_TWO ..."
SED_PREFIX=${PREFIX//\//\\\/} #Replace forward slashes in $PREFIX with \/ so that sed doesn't freak out
sed "s/SCRIPT_PATH/$SED_PREFIX\\/bin\\/sylpheed-kdeservicemenu.pl/" $DESKTOP_TEMPLATE_TWO > $DESKTOP_TWO
cp -f $DESKTOP_TWO $PREFIX/share/apps/konqueror/servicemenus/sylpheed-compress-attach.desktop
cp -f $PERL_SCRIPT $PREFIX/bin/
echo "Setting permissions ..."
chmod 0644 $PREFIX/share/apps/konqueror/servicemenus/sylpheed-attach-files.desktop
chmod 0644 $PREFIX/share/apps/konqueror/servicemenus/sylpheed-compress-attach.desktop
chmod 0755 $PREFIX/bin/$PERL_SCRIPT
}

case $1 in
  "--global" )
    check_environ
    PREFIX=$(kde-config --prefix)
    echo "Installing in $PREFIX ..."
    if [ "$(id -u)" != "0" ]; then
      echo "You are not root, as would be expected."
      echo "However, we will still attempt a system-wide install."
      echo "But, you probably don't have permission."
    fi
    install_all
    ;;
  "--local" )
    check_environ
    PREFIX=$(kde-config --localprefix)
    echo "Installing in $PREFIX ..."
    if [ ! -d $PREFIX/bin ]; then
      mkdir $PREFIX/bin
    fi
    if [ ! -d $PREFIX/share/apps/konqueror/servicemenus ]; then
      mkdir $PREFIX/share/apps/konqueror/servicemenus
    fi
    install_all
    ;;
  * )
    echo "Usage: $0 [--global|--local]"
    echo
    echo "--global  attempts a system-wide installation."
    echo "--local   attempts to install in your home directory."
    echo
    exit 0
    ;;
esac

echo "Done."
