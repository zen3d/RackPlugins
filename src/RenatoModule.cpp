#include "Renato.hpp"
#include <sstream>


bool Access(Renato *pr, bool is_x, int p) { return is_x ? pr->_accessX(p) : pr->_accessY(p); }

void Renato::led(int l) 
{
	for(int r = 0; r < 4; r++)
	{
		for(int c = 0; c < 4; c++)
		{
			int n = c + r * 4;
			lights[LED_1 + n].value = l == n ? LED_ON : LED_OFF; 
		}
	}
}

void Renato::setOut(int l, bool on)
{
	for (int r = 0; r < 4; r++)
	{
		for (int c = 0; c < 4; c++)
		{
			int n = c + r * 4;
			lights[LED_1 + n].value = l == n ? LED_ON : LED_OFF;
			outputs[CV_OUTSTEP1 + n].value = (on && l == n) ? LVL_ON : LVL_OFF;
		}
	}
}
void Renato::on_loaded()
{
	#ifdef DIGITAL_EXT
	connected = 0;
	#endif
	load();
}

void Renato::load()
{
	seqX.Reset();
	seqY.Reset();
}
void Renato::process(const ProcessArgs &args)
{
	if(resetTrigger.process(inputs[RESET].value) || masterReset.process(params[M_RESET].value))
	{
		seqX.Reset();
		seqY.Reset();
	} else
	{
		if (pWidget != NULL)
		{
			if (accessRndTrigger.process(inputs[ACCESS_RND].value))
				pWidget->std_randomize(Renato::ACCESS_1, Renato::ACCESS_1 + 16);
			if (gatexRndTrigger.process(inputs[GATEX_RND].value))
				pWidget->std_randomize(Renato::GATEX_1, Renato::GATEX_1 + 16);
			if (gateyRndTrigger.process(inputs[GATEY_RND].value))
				pWidget->std_randomize(Renato::GATEY_1, Renato::GATEY_1 + 16);
		}
		bool seek_mode = params[SEEKSLEEP].value > 0;
		int clkX = seqX.Step(inputs[XCLK].value, params[COUNTMODE_X].value, seek_mode, this, true);
		int clkY = seqY.Step(inputs[YCLK].value, params[COUNTMODE_Y].value, seek_mode, this, false);
		int n = xy(seqX.Position(), seqY.Position());
		
		if(_access(n) )
		{
			bool on = false;
			if(_gateX(n))
			{
				if(seqX.Gate(clkX, &outputs[XGATE], &lights[LED_GATEX]))
					on = true;
			}

			if(_gateY(n))
			{
				if(seqY.Gate(clkY, &outputs[YGATE], &lights[LED_GATEY]))
					on = true;
			}

			outputs[CV].value = params[VOLTAGE_1 + n].value;
			setOut(n, on);
			led(n);
		}
	}
	#ifdef DIGITAL_EXT
	bool dig_connected = false;

	#ifdef LAUNCHPAD
	if(drv->Connected())
		dig_connected = true;
	drv->ProcessLaunchpad();
	#endif

	#if defined(OSCTEST_MODULE)
	if(oscDrv->Connected())
		dig_connected = true;
	oscDrv->ProcessOSC();
	#endif	
	connected = dig_connected ? 1.0 : 0.0;
	#endif
}

Menu *RenatoWidget::addContextMenu(Menu *menu)
{
	menu->addChild(new SeqMenuItem<RenatoWidget>("Randomize Pitch", this, RANDOMIZE_PITCH));
	menu->addChild(new SeqMenuItem<RenatoWidget>("Randomize Gate Xs", this, RANDOMIZE_GATEX));
	menu->addChild(new SeqMenuItem<RenatoWidget>("Randomize Gate Ys", this, RANDOMIZE_GATEY));
	menu->addChild(new SeqMenuItem<RenatoWidget>("Randomize Access", this, RANDOMIZE_ACCESS));
	return menu;
}

void RenatoWidget::onMenu(int action)
{
	switch(action)
	{
	case RANDOMIZE_PITCH: std_randomize(Renato::VOLTAGE_1, Renato::VOLTAGE_1 + 16); break;
	case RANDOMIZE_GATEX: std_randomize(Renato::GATEX_1, Renato::GATEX_1 + 16); break;
	case RANDOMIZE_GATEY: std_randomize(Renato::GATEY_1, Renato::GATEY_1 + 16); break;
	case RANDOMIZE_ACCESS: std_randomize(Renato::ACCESS_1, Renato::ACCESS_1 + 16); break;
	}
}

RenatoWidget::RenatoWidget(Renato *module ) : SequencerWidget(module)
{
	if (module != NULL)
		module->setWidget(this);

	#ifdef OSCTEST_MODULE
	char name[60];
	#endif

	box.size = Vec(39 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
	SvgPanel *panel = new SvgPanel();
	panel->box.size = box.size;
	panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/modules/RenatoModule.svg")));
	addChild(panel);
	addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
	addChild(createWidget<ScrewBlack>(Vec(box.size.x -  2*RACK_GRID_WIDTH, 0)));
	addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, box.size.y - RACK_GRID_WIDTH)));
	addChild(createWidget<ScrewBlack>(Vec(box.size.x -  2*RACK_GRID_WIDTH, box.size.y - RACK_GRID_WIDTH)));

	addInput(createInput<PJ301RPort>(Vec(mm2px(39.330), yncscape(115.267,8.255)), module, Renato::XCLK));
	addInput(createInput<PJ301RPort>(Vec(mm2px(55.242), yncscape(115.267,8.255)), module, Renato::YCLK));
	addInput(createInput<PJ301YPort>(Vec(mm2px(133.987), yncscape(115.267,8.255)), module, Renato::RESET));

	addInput(createInput<PJ301BPort>(Vec(mm2px(7.899), yncscape(115.267, 8.255)), module, Renato::ACCESS_RND));
	addInput(createInput<PJ301BPort>(Vec(mm2px(18.293), yncscape(115.267, 8.255)), module, Renato::GATEX_RND));
	addInput(createInput<PJ301BPort>(Vec(mm2px(28.687), yncscape(115.267, 8.255)), module, Renato::GATEY_RND));
	
	addChild(createParam<BefacoPushBig>(Vec(mm2px(122.982), yncscape(114.895, 8.999)), module, Renato::M_RESET));

	// page 0 (SESSION)
	ParamWidget *pwdg = createParam<NKK2>(Vec(mm2px(71.102), yncscape(115.727+1, 8.467)), module, Renato::COUNTMODE_X);
	addParam(pwdg);
	#ifdef LAUNCHPAD
	if(module != NULL)
	{
		LaunchpadRadio *radio = new LaunchpadRadio(0, ILaunchpadPro::RC2Key(2, 1), 3, LaunchpadLed::Color(47), LaunchpadLed::Color(32));
		module->drv->Add(radio, pwdg);
	}
	#endif
	#ifdef OSCTEST_MODULE
	if(module != NULL)
	{
		module->oscDrv->Add(new oscControl("/ModeX"), pwdg);
	}
	#endif

	pwdg = createParam<NKK2>(Vec(mm2px(94.827), yncscape(115.727+1, 8.467)), module, Renato::COUNTMODE_Y);
	addParam(pwdg);
	#ifdef LAUNCHPAD
	if(module != NULL)
	{
		module->drv->Add(new LaunchpadRadio(0, ILaunchpadPro::RC2Key(2, 3), 3, LaunchpadLed::Color(19), LaunchpadLed::Color(21)), pwdg);
	}
	#endif
	#ifdef OSCTEST_MODULE
	if(module != NULL)
	{
		module->oscDrv->Add(new oscControl("/ModeY"), pwdg);
	}
	#endif

	pwdg = createParam<NKK1>(Vec(mm2px(109.558), yncscape(115.727+1, 8.467)), module, Renato::SEEKSLEEP);
	addParam(pwdg);
	#ifdef LAUNCHPAD
	if(module != NULL)
	{
		module->drv->Add(new LaunchpadRadio(0, ILaunchpadPro::RC2Key(2, 5), 2, LaunchpadLed::Color(51), LaunchpadLed::Color(52)), pwdg);
	}
	#endif
	#ifdef OSCTEST_MODULE
	if(module != NULL)
	{
		module->oscDrv->Add(new oscControl("/Seek"), pwdg);
	}
	#endif

	addOutput(createOutput<PJ301GPort>(Vec(mm2px(181.436), yncscape(115.267, 8.255)), module, Renato::CV));
	addOutput(createOutput<PJ301WPort>(Vec(mm2px(150.245), yncscape(115.267, 8.255)), module, Renato::XGATE));
	ModuleLightWidget *plight = createLight<MediumLight<GreenLight>>(Vec(mm2px(157.888), yncscape(112.637, 3.176)), module, Renato::LED_GATEX);
	#ifdef OSCTEST_MODULE
	if(module != NULL)
	{
		module->oscDrv->Add(new oscControl("/LedGX"), plight);
	}
	#endif
	addChild(plight);

	addOutput(createOutput<PJ301WPort>(Vec(mm2px(171.033), yncscape(115.267, 8.255)), module, Renato::YGATE));
	plight = createLight<MediumLight<GreenLight>>(Vec(mm2px(168.403), yncscape(112.637, 3.176)), module, Renato::LED_GATEY);
	#ifdef OSCTEST_MODULE
	if(module != NULL)
	{
		module->oscDrv->Add(new oscControl("/LedGY"), plight);
	}
	#endif
	addChild(plight);

	// page 1 (NOTES)
	float groupdist_h = 47.343;
	float groupdist_v = -27.8;
	float x_sup[4] = {7.899, 18.293, 28.687, 39.330};
	float x_inf[4] = {9.788, 20.182, 30.576, 38.695};
	float x_led = 46.981;
	float y_led = 97.848;
	float y_sup = 100.419;
	float y_inf = 92.021;
	float y_pot = 89.561;

	for(int r = 0; r < 4; r++)
	{
		for(int c = 0; c < 4; c++)
		{
			int n = c + r * 4;
			addInput(createInput<PJ301BPort>(Vec(mm2px(x_sup[0]+c* groupdist_h), yncscape(y_sup + r * groupdist_v, 8.255)), module, Renato::ACCESS_IN1+n));
			addInput(createInput<PJ301BPort>(Vec(mm2px(x_sup[1] + c * groupdist_h), yncscape(y_sup+r*groupdist_v, 8.255)), module, Renato::GATEX_IN1 + n));
			addInput(createInput<PJ301BPort>(Vec(mm2px(x_sup[2] + c * groupdist_h), yncscape(y_sup+r*groupdist_v, 8.255)), module, Renato::GATEY_IN1 + n));
			addOutput(createOutput<PJ301WPort>(Vec(mm2px(x_sup[3] + c * groupdist_h), yncscape(y_sup+r*groupdist_v, 8.255)), module, Renato::CV_OUTSTEP1 + n));

			ParamWidget *pwdg = createParam<TL1105Sw>(Vec(mm2px(x_inf[0]+c*groupdist_h), yncscape(y_inf + r * groupdist_v, 6.607)), module, Renato::ACCESS_1 + n);
			addParam(pwdg);
			#ifdef LAUNCHPAD
			if(module != NULL)
			{
				module->drv->Add(new LaunchpadSwitch(1, ILaunchpadPro::RC2Key(r + 4, c), LaunchpadLed::Off(), LaunchpadLed::Color(17)), pwdg);
			}
			#endif
			#ifdef OSCTEST_MODULE
			if(module != NULL)
			{
				sprintf(name, "/Access%i", n + 1);
				module->oscDrv->Add(new oscControl(name), pwdg);
			}
			#endif

			pwdg = createParam<TL1105Sw>(Vec(mm2px(x_inf[1] + c * groupdist_h), yncscape(y_inf + r * groupdist_v, 6.607)), module, Renato::GATEX_1 + n);
			addParam(pwdg);
			#ifdef LAUNCHPAD
			if(module != NULL)
			{
				module->drv->Add(new LaunchpadSwitch(1, ILaunchpadPro::RC2Key(r + 4, c + 4), LaunchpadLed::Off(), LaunchpadLed::Color(52)), pwdg);
			}
			#endif
			#ifdef OSCTEST_MODULE
			if(module != NULL)
			{
				sprintf(name, "/GateX%i", n + 1);
				module->oscDrv->Add(new oscControl(name), pwdg);
			}
			#endif

			pwdg = createParam<TL1105Sw>(Vec(mm2px(x_inf[2] + c * groupdist_h), yncscape(y_inf + r * groupdist_v, 6.607)), module, Renato::GATEY_1 + n);
			addParam(pwdg);
			#ifdef LAUNCHPAD
			if(module != NULL)
			{
				module->drv->Add(new LaunchpadSwitch(1, ILaunchpadPro::RC2Key(r, c + 4), LaunchpadLed::Off(), LaunchpadLed::Color(62)), pwdg);
			}
			#endif
			#ifdef OSCTEST_MODULE
			if(module != NULL)
			{
				sprintf(name, "/GateY%i", n + 1);
				module->oscDrv->Add(new oscControl(name), pwdg);
			}
			#endif
			
			pwdg = createParam<Davies1900hFixRedKnob>(Vec(mm2px(x_inf[3] + c * groupdist_h), yncscape(y_pot + r * groupdist_v, 9.525)), module, Renato::VOLTAGE_1 + n);
			#ifdef OSCTEST_MODULE
			if(module != NULL)
			{
				sprintf(name, "/Knob%i", n+1);
				module->oscDrv->Add(new oscControl(name), pwdg);
			}
			#endif
			addParam(pwdg);

			ModuleLightWidget *plight = createLight<MediumLight<RedLight>>(Vec(mm2px(x_led + c * groupdist_h), yncscape(y_led+r*groupdist_v, 3.176)), module, Renato::LED_1 + n);
			addChild(plight);
			#ifdef LAUNCHPAD
			if(module != NULL)
			{
				LaunchpadLight *ld1 = new LaunchpadLight(1, ILaunchpadPro::RC2Key(r, c), LaunchpadLed::Off(), LaunchpadLed::Color(4));
				module->drv->Add(ld1, plight);
			}
			#endif
			#ifdef OSCTEST_MODULE
			if(module != NULL)
			{
				sprintf(name, "/Led%i", n + 1);
				module->oscDrv->Add(new oscControl(name), plight);
			}
			#endif
		}
	}
	
	#ifdef DIGITAL_EXT
	if(module != NULL)
		addChild(new DigitalLed(mm2px(107.531), yncscape(117.461, 3.867), &module->connected));
	#endif
}
