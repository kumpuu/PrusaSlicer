#include "Config.hpp"

namespace Slic3r {

bool
ConfigBase::has(const t_config_option_key opt_key) {
    return (this->option(opt_key, false) != NULL);
}

void
ConfigBase::apply(ConfigBase &other, bool ignore_nonexistent) {
    // get list of option keys to apply
    t_config_option_keys opt_keys;
    other.keys(&opt_keys);
    
    // loop through options and apply them
    for (t_config_option_keys::const_iterator it = opt_keys.begin(); it != opt_keys.end(); ++it) {
        ConfigOption* my_opt = this->option(*it, true);
        if (my_opt == NULL) {
            if (ignore_nonexistent == false) throw "Attempt to apply non-existent option";
            continue;
        }
        
        // not the most efficient way, but easier than casting pointers to subclasses
        my_opt->deserialize( other.option(*it)->serialize() );
    }
}

std::string
ConfigBase::serialize(const t_config_option_key opt_key) {
    ConfigOption* opt = this->option(opt_key);
    assert(opt != NULL);
    return opt->serialize();
}

void
ConfigBase::set_deserialize(const t_config_option_key opt_key, std::string str) {
    ConfigOption* opt = this->option(opt_key);
    assert(opt != NULL);
    opt->deserialize(str);
}

double
ConfigBase::get_abs_value(const t_config_option_key opt_key) {
    // get option definition
    assert(this->def->count(opt_key) != 0);
    ConfigOptionDef* def = &(*this->def)[opt_key];
    assert(def->type == coFloatOrPercent);
    
    // get stored option value
    ConfigOptionFloatOrPercent* opt = dynamic_cast<ConfigOptionFloatOrPercent*>(this->option(opt_key));
    assert(opt != NULL);
    
    // compute absolute value
    if (opt->percent) {
        ConfigOptionFloat* optbase = dynamic_cast<ConfigOptionFloat*>(this->option(def->ratio_over));
        if (optbase == NULL) throw "ratio_over option not found";
        return optbase->value * opt->value / 100;
    } else {
        return opt->value;
    }
}

#ifdef SLIC3RXS
SV*
ConfigBase::as_hash() {
    HV* hv = newHV();
    
    t_config_option_keys opt_keys;
    this->keys(&opt_keys);
    
    for (t_config_option_keys::const_iterator it = opt_keys.begin(); it != opt_keys.end(); ++it)
        (void)hv_store( hv, it->c_str(), it->length(), this->get(*it), 0 );
    
    return newRV_noinc((SV*)hv);
}

SV*
ConfigBase::get(t_config_option_key opt_key) {
    ConfigOption* opt = this->option(opt_key);
    if (opt == NULL) return &PL_sv_undef;
    if (ConfigOptionFloat* optv = dynamic_cast<ConfigOptionFloat*>(opt)) {
        return newSVnv(optv->value);
    } else if (ConfigOptionFloats* optv = dynamic_cast<ConfigOptionFloats*>(opt)) {
        AV* av = newAV();
        av_fill(av, optv->values.size()-1);
        for (std::vector<double>::iterator it = optv->values.begin(); it != optv->values.end(); ++it)
            av_store(av, it - optv->values.begin(), newSVnv(*it));
        return newRV_noinc((SV*)av);
    } else if (ConfigOptionInt* optv = dynamic_cast<ConfigOptionInt*>(opt)) {
        return newSViv(optv->value);
    } else if (ConfigOptionInts* optv = dynamic_cast<ConfigOptionInts*>(opt)) {
        AV* av = newAV();
        av_fill(av, optv->values.size()-1);
        for (std::vector<int>::iterator it = optv->values.begin(); it != optv->values.end(); ++it)
            av_store(av, it - optv->values.begin(), newSViv(*it));
        return newRV_noinc((SV*)av);
    } else if (ConfigOptionString* optv = dynamic_cast<ConfigOptionString*>(opt)) {
        // we don't serialize() because that would escape newlines
        return newSVpvn(optv->value.c_str(), optv->value.length());
    } else if (ConfigOptionStrings* optv = dynamic_cast<ConfigOptionStrings*>(opt)) {
        AV* av = newAV();
        av_fill(av, optv->values.size()-1);
        for (std::vector<std::string>::iterator it = optv->values.begin(); it != optv->values.end(); ++it)
            av_store(av, it - optv->values.begin(), newSVpvn(it->c_str(), it->length()));
        return newRV_noinc((SV*)av);
    } else if (ConfigOptionPoint* optv = dynamic_cast<ConfigOptionPoint*>(opt)) {
        return optv->point.to_SV_pureperl();
    } else if (ConfigOptionPoints* optv = dynamic_cast<ConfigOptionPoints*>(opt)) {
        AV* av = newAV();
        av_fill(av, optv->points.size()-1);
        for (Pointfs::iterator it = optv->points.begin(); it != optv->points.end(); ++it)
            av_store(av, it - optv->points.begin(), it->to_SV_pureperl());
        return newRV_noinc((SV*)av);
    } else if (ConfigOptionBool* optv = dynamic_cast<ConfigOptionBool*>(opt)) {
        return newSViv(optv->value ? 1 : 0);
    } else if (ConfigOptionBools* optv = dynamic_cast<ConfigOptionBools*>(opt)) {
        AV* av = newAV();
        av_fill(av, optv->values.size()-1);
        for (std::vector<bool>::iterator it = optv->values.begin(); it != optv->values.end(); ++it)
            av_store(av, it - optv->values.begin(), newSViv(*it ? 1 : 0));
        return newRV_noinc((SV*)av);
    } else {
        std::string serialized = opt->serialize();
        return newSVpvn(serialized.c_str(), serialized.length());
    }
}

void
ConfigBase::set(t_config_option_key opt_key, SV* value) {
    ConfigOption* opt = this->option(opt_key, true);
    if (opt == NULL) CONFESS("Trying to set non-existing option");
    
    if (ConfigOptionFloat* optv = dynamic_cast<ConfigOptionFloat*>(opt)) {
        optv->value = SvNV(value);
    } else if (ConfigOptionFloats* optv = dynamic_cast<ConfigOptionFloats*>(opt)) {
        optv->values.clear();
        AV* av = (AV*)SvRV(value);
        const size_t len = av_len(av)+1;
        for (size_t i = 0; i < len; i++) {
            SV** elem = av_fetch(av, i, 0);
            optv->values.push_back(SvNV(*elem));
        }
    } else if (ConfigOptionInt* optv = dynamic_cast<ConfigOptionInt*>(opt)) {
        optv->value = SvIV(value);
    } else if (ConfigOptionInts* optv = dynamic_cast<ConfigOptionInts*>(opt)) {
        optv->values.clear();
        AV* av = (AV*)SvRV(value);
        const size_t len = av_len(av)+1;
        for (size_t i = 0; i < len; i++) {
            SV** elem = av_fetch(av, i, 0);
            optv->values.push_back(SvIV(*elem));
        }
    } else if (ConfigOptionString* optv = dynamic_cast<ConfigOptionString*>(opt)) {
        optv->value = std::string(SvPV_nolen(value), SvCUR(value));
    } else if (ConfigOptionStrings* optv = dynamic_cast<ConfigOptionStrings*>(opt)) {
        optv->values.clear();
        AV* av = (AV*)SvRV(value);
        const size_t len = av_len(av)+1;
        for (size_t i = 0; i < len; i++) {
            SV** elem = av_fetch(av, i, 0);
            optv->values.push_back(std::string(SvPV_nolen(*elem), SvCUR(*elem)));
        }
    } else if (ConfigOptionPoint* optv = dynamic_cast<ConfigOptionPoint*>(opt)) {
        optv->point.from_SV(value);
    } else if (ConfigOptionPoints* optv = dynamic_cast<ConfigOptionPoints*>(opt)) {
        optv->points.clear();
        AV* av = (AV*)SvRV(value);
        const size_t len = av_len(av)+1;
        for (size_t i = 0; i < len; i++) {
            SV** elem = av_fetch(av, i, 0);
            Pointf point;
            point.from_SV(*elem);
            optv->points.push_back(point);
        }
    } else if (ConfigOptionBool* optv = dynamic_cast<ConfigOptionBool*>(opt)) {
        optv->value = SvTRUE(value);
    } else if (ConfigOptionBools* optv = dynamic_cast<ConfigOptionBools*>(opt)) {
        optv->values.clear();
        AV* av = (AV*)SvRV(value);
        const size_t len = av_len(av)+1;
        for (size_t i = 0; i < len; i++) {
            SV** elem = av_fetch(av, i, 0);
            optv->values.push_back(SvTRUE(*elem));
        }
    } else {
        opt->deserialize( std::string(SvPV_nolen(value)) );
    }
}
#endif

DynamicConfig::~DynamicConfig () {
    for (t_options_map::iterator it = this->options.begin(); it != this->options.end(); ++it) {
        ConfigOption* opt = it->second;
        if (opt != NULL) delete opt;
    }
}

ConfigOption*
DynamicConfig::option(const t_config_option_key opt_key, bool create) {
    if (this->options.count(opt_key) == 0) {
        if (create) {
            ConfigOptionDef* optdef = &(*this->def)[opt_key];
            ConfigOption* opt;
            if (optdef->type == coFloat) {
                opt = new ConfigOptionFloat ();
            } else if (optdef->type == coFloats) {
                opt = new ConfigOptionFloats ();
            } else if (optdef->type == coInt) {
                opt = new ConfigOptionInt ();
            } else if (optdef->type == coInts) {
                opt = new ConfigOptionInts ();
            } else if (optdef->type == coString) {
                opt = new ConfigOptionString ();
            } else if (optdef->type == coStrings) {
                opt = new ConfigOptionStrings ();
            } else if (optdef->type == coFloatOrPercent) {
                opt = new ConfigOptionFloatOrPercent ();
            } else if (optdef->type == coPoint) {
                opt = new ConfigOptionPoint ();
            } else if (optdef->type == coPoints) {
                opt = new ConfigOptionPoints ();
            } else if (optdef->type == coBool) {
                opt = new ConfigOptionBool ();
            } else if (optdef->type == coBools) {
                opt = new ConfigOptionBools ();
            } else if (optdef->type == coEnum) {
                ConfigOptionEnumGeneric* optv = new ConfigOptionEnumGeneric ();
                optv->keys_map = &optdef->enum_keys_map;
                opt = static_cast<ConfigOption*>(optv);
            } else {
                throw "Unknown option type";
            }
            this->options[opt_key] = opt;
            return opt;
        } else {
            return NULL;
        }
    }
    return this->options[opt_key];
}

void
DynamicConfig::keys(t_config_option_keys *keys) {
    for (t_options_map::const_iterator it = this->options.begin(); it != this->options.end(); ++it)
        keys->push_back(it->first);
}

void
StaticConfig::keys(t_config_option_keys *keys) {
    for (t_optiondef_map::const_iterator it = this->def->begin(); it != this->def->end(); ++it) {
        ConfigOption* opt = this->option(it->first);
        if (opt != NULL) keys->push_back(it->first);
    }
}

}
