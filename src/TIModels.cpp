
/*
 * Part of tivars_lib_cpp
 * (C) 2015 Adrien 'Adriweb' Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

namespace tivars;

abstract class TIFeatureFlags
{
    const has82things  = 0b00000001; // (1 << 0);
    const hasComplex   = 0b00000010; // (1 << 1);
    const hasFlash     = 0b00000100; // (1 << 2);
    const hasApps      = 0b00001000; // (1 << 3);
    const hasClock     = 0b00010000; // (1 << 4);
    const hasColorLCD  = 0b00100000; // (1 << 5);
    const hasEZ80CPU   = 0b01000000; // (1 << 6);
    const hasExactMath = 0b10000000; // (1 << 7);
}

abstract class TIModels
{
    private static $models = [];

    /**
     *  Make and insert the associative arrays for the model.
     *
     * @param int|null  $orderID The orderID (for the extensions association)
     * @param int       $flags   The flags determining available features
     * @param string    $name    The name of the calc using this model
     * @param string    $sig     The signature (magic bytes) used for this model
     */
    private static function insertModel($orderID, $flags, $name, $sig)
    {
        if (!isset(self::$models[$name]))
            self::$models[$name]  = ['orderID' => $orderID, 'flags' => $flags, 'sig' => $sig];

        if (!isset(self::$models[$flags]))
            self::$models[$flags] = ['orderID' => $orderID, 'name' => $name, 'sig' => $sig];

        if (!isset(self::$models[$sig]))
            self::$models[$sig]   = ['orderID' => $orderID, 'flags' => $flags, 'name' => $name];
    }

    // TODO : Research actual compatibility flags/"versions" from libtifiles, and maybe even TI ?
    public static function initTIModelsArray()
    {
        $flags82     = 0            | TIFeatureFlags::has82things;
        $flags83     = $flags82     | TIFeatureFlags::hasComplex;
        $flags82a    = $flags83     | TIFeatureFlags::hasFlash;
        $flags83p    = $flags82a    | TIFeatureFlags::hasApps;
        $flags84p    = $flags83p    | TIFeatureFlags::hasClock;
        $flags84pcse = $flags84p    | TIFeatureFlags::hasColorLCD;
        $flags84pce  = $flags84pcse | TIFeatureFlags::hasEZ80CPU;
        $flags83pce  = $flags84pce  | TIFeatureFlags::hasExactMath;

        self::insertModel(-1,   0,            'Unknown', '');
        self::insertModel(0,    $flags82,     '82',      '**TI82**');
        self::insertModel(1,    $flags83,     '83',      '**TI83**');
        self::insertModel(2,    $flags82a,    '82A',     '**TI83F*');
        self::insertModel(3,    $flags83p,    '82+',     '**TI83F*');
        self::insertModel(3,    $flags83p,    '83+',     '**TI83F*');
        self::insertModel(3,    $flags84p,    '84+',     '**TI83F*');
        self::insertModel(4,    $flags84pcse, '84+CSE',  '**TI83F*');
        self::insertModel(5,    $flags84pce,  '84+CE',   '**TI83F*');
        self::insertModel(6,    $flags83pce,  '83PCE',   '**TI83F*');
    }

    /**
     * @param   int     $flags  The model flags
     * @return  string          The model name for those flags
     */
    public static function getDefaultNameFromFlags($flags = 0)
    {
        return self::isValidFlags($flags) ? self::$models[$flags]['name'] : 'Unknown';
    }

    /**
     * @param   string  $name   The model name
     * @return  int             The model flags for that name
     */
    public static function getFlagsFromName($name = '')
    {
        return self::isValidName($name) ? self::$models[$name]['flags'] : 0;
    }

    /**
     * @param   int     $flags  The model flags
     * @return  string          The signature for those flags
     */
    public static function getSignatureFromFlags($flags = 0)
    {
        return self::isValidFlags($flags) ? self::$models[$flags]['sig'] : '';
    }

    /**
     * @param   string  $name
     * @return  string          The signature for that name
     */
    public static function getSignatureFromName($name = '')
    {
        return self::isValidName($name) ? self::$models[$name]['sig'] : '';
    }

    /**
     * @param   string  $sig    The signature
     * @return  string          The default calc name whose file formats use that signature
     */
    public static function getDefaultNameFromSignature($sig = '')
    {
        return self::isValidSignature($sig) ? self::$models[$sig]['name'] : '';
    }

    /**
     * @param   string  $sig    The signature
     * @return  int             The default calc order ID whose file formats use that signature
     */
    public static function getDefaultOrderIDFromSignature($sig = '')
    {
        return self::isValidSignature($sig) ? self::$models[$sig]['orderID'] : -1;
    }

    /**
     * @param   string  $name
     * @return  int             The default calc order ID whose file formats use that signature
     */
    public static function getOrderIDFromName($name = '')
    {
        return self::isValidName($name) ? self::$models[$name]['orderID'] : -1;
    }

    /**
     * @param   int     $flags  The model flags
     * @return  int             The default calc order ID whose file formats use that signature
     */
    public static function getDefaulOrderIDFromFlags($flags = 0)
    {
        return self::isValidFlags($flags) ? self::$models[$flags]['orderID'] : -1;
    }

    /**
     * @param   string  $sig    The signature
     * @return  string          The minimum compatibility flags for that signaure
     */
    public static function getMinFlagsFromSignature($sig = '')
    {
        return self::isValidSignature($sig) ? self::$models[$sig]['flags'] : 0;
    }


    public static function isValidFlags($flags = 0)
    {
        return ($flags !== 0 && isset(self::$models[$flags]));
    }

    public static function isValidName($name = '')
    {
        return ($name !== '' && isset(self::$models[$name]));
    }

    public static function isValidSignature($sig = '')
    {
        return ($sig !== '' && isset(self::$models[$sig]));
    }
}

TIModels::initTIModelsArray();