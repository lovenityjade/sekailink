window.addEventListener('load', () => {
  // Mobile menu handling
  const menuButton = document.getElementById('base-header-mobile-menu-button');
  const mobileMenu = document.getElementById('base-header-mobile-menu');

  menuButton.addEventListener('click', (evt) => {
    evt.preventDefault();
    evt.stopPropagation();

    if (!mobileMenu.style.display || mobileMenu.style.display === 'none') {
      return mobileMenu.style.display = 'flex';
    }

    mobileMenu.style.display = 'none';
  });

  window.addEventListener('resize', () => {
    mobileMenu.style.display = 'none';
  });

  // Popover handling
  const popoverText = document.getElementById('base-header-popover-text');
  const popoverMenu = document.getElementById('base-header-popover-menu');
  const popoverText2 = document.getElementById('base-header-popover-text-2');
  const popoverMenu2 = document.getElementById('base-header-popover-menu-2');

  popoverText.addEventListener('click', (evt) => {
    evt.preventDefault();
    evt.stopPropagation();

    if (!popoverMenu.style.display || popoverMenu.style.display === 'none') {

      if (popoverMenu2.style.display && popoverMenu2.style.display === 'flex') {
        popoverMenu2.style.display = 'none';
      }
      return popoverMenu.style.display = 'flex';
    }

    popoverMenu.style.display = 'none';
  });

  document.body.addEventListener('click', () => {
    mobileMenu.style.display = 'none';
    popoverMenu.style.display = 'none';
  });


  
    popoverText2.addEventListener('click', (evt) => {
      evt.preventDefault();
      evt.stopPropagation();
  
      if (!popoverMenu2.style.display || popoverMenu2.style.display === 'none') {

        if (popoverMenu.style.display && popoverMenu.style.display === 'flex') {
          popoverMenu.style.display = 'none';
        }

        return popoverMenu2.style.display = 'flex';
      }
  
      popoverMenu2.style.display = 'none';
    });
  
    document.body.addEventListener('click', () => {
      mobileMenu.style.display = 'none';
      popoverMenu.style.display = 'none';
      popoverMenu2.style.display = 'none';
    });
});
