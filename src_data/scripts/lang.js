class Lang {
  constructor(switcher, defaultLocale = null) {
    if (!(switcher instanceof Object)) {
      throw new SyntaxError("switcher must be an element object");
    }

    this.switcher = switcher;
    this.defaultLocale = defaultLocale;
    this.supportedLocales = [];
    this.currentLocale = null;
  }

  async build() {
    this.bindSwitcher();
  
    const userLocale = localStorage.getItem('locale');
    if (this.localeIsSupported(userLocale)) {
      await this.setLocale(userLocale);
  
    } else {
      const initialLocale = this.getSuitableLocale(this.browserLocales(true));
      await this.setLocale(initialLocale);
    }

    this.translatePage();
  }

  bindSwitcher() {
    this.supportedLocales = [];
    for (const option of this.switcher.options) {
      this.supportedLocales.push(option.value);
    }

    if (!this.localeIsSupported(this.defaultLocale)) {
      const selected = this.switcher.selectedIndex ?? 0;
      this.defaultLocale = this.switcher.options[selected].value;
    }
  
    this.switcher.addEventListener('change', async (element) => {
      await this.setLocale(element.target.value);
      this.translatePage();
    });
  }

  async setLocale(newLocale) {
    if (this.currentLocale == newLocale) {
      return;
    }

    i18n.translator.reset();
    i18n.translator.add(await this.fetchTranslations(newLocale));
  
    this.currentLocale = newLocale;
    localStorage.setItem('locale', this.currentLocale);
  
    if (document.documentElement) {
      document.documentElement.setAttribute("lang", this.currentLocale);
    }

    if (this.switcher.value != this.currentLocale) {
      this.switcher.value = this.currentLocale;
    }
  }

  async fetchTranslations(locale) {
    const response = await fetch(`/static/locales/${locale}.json`);
    const data = await response.json();
  
    if (data.values instanceof Object) {
      data.values = this.flattenKeys({keys: data.values, prefix: ''});
    }
  
    return data;
  }

  translatePage() {
    document
      .querySelectorAll("[data-i18n]")
      .forEach((element) => this.translateElement(element));
  }
  
  translateElement(element) {
    let key = element.getAttribute("data-i18n");
    if (!key && element.innerHTML) {
      key = element.innerHTML;
      element.setAttribute("data-i18n", key);
    }

    if (!key) {
      return;
    }
  
    const arg = element.getAttribute("data-i18n-arg") || null;
    const options = JSON.parse(element.getAttribute("data-i18n-options")) || null;
  
    element.innerHTML = i18n(key, arg, options);
  }


  localeIsSupported(locale) {
    return locale !== null && this.supportedLocales.indexOf(locale) > -1;
  }
  
  getSuitableLocale(locales) {
    return locales.find(this.localeIsSupported, this) || this.defaultLocale;
  }
  
  browserLocales(codeOnly = false) {
    return navigator.languages.map((locale) =>
      codeOnly ? locale.split("-")[0] : locale,
    );
  }

  flattenKeys({ keys, prefix }) {
    let result = {};
    for (let key in keys) {
      const type = typeof keys[key];
      if (type === 'string') {
        result[`${prefix}${key}`] = keys[key];
      }
      else if (type === 'object') {
        result = { ...result, ...this.flattenKeys({ keys: keys[key], prefix: `${prefix}${key}.` }) }
      }
    }
    return result;
  }
}

