
/*
 * Part of tivars_lib_cpp
 * (C) 2015 Adrien 'Adriweb' Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

namespace tivars;

abstract class TIVarTypes
{
    private static $types = [];

    /**
     *  Make and insert the associative arrays for the type.
     *
     * @param string    $name   The name of the type
     * @param int       $id     The ID of the type
     * @param array     $exts   The extensions the type can have, ordered by feature flags.
     */
    private static function insertType($name, $id, array $exts)
    {
        self::$types[$name]    = [ 'id'   => $id,   'exts' => $exts ];
        self::$types[$id]      = [ 'name' => $name, 'exts' => $exts ];
        foreach ($exts as $ext)
        {
            if ($ext !== null && !isset(self::$types[$ext]))
            {
                self::$types[$ext] = ['id' => $id, 'name' => $name];
            }
        }
    }

    // 82+/83+/84+ are grouped since only the clock is the difference, and it doesn't have an actual varType.
    public static function initTIVarTypesArray() // order: 82     83   82A 82+/83+/84+ 84+CSE 84+CE 83PCE
    {
        self::insertType('Unknown',                -1,  [ null,  null,  null,  null,  null,  null,  null]);

        /* Standard types */
        self::insertType('Real',                 0x00,  ['82n', '83n', '8xn', '8xn', '8xn', '8xn', '8xn']);
        self::insertType('RealList',             0x01,  ['82l', '83l', '8xl', '8xl', '8xl', '8xl', '8xl']);
        self::insertType('Matrix',               0x02,  ['82m', '83m', '8xm', '8xm', '8xm', '8xm', '8xm']);
        self::insertType('Equation',             0x03,  ['82y', '83y', '8xy', '8xy', '8xy', '8xy', '8xy']);
        self::insertType('String',               0x04,  ['82s', '83s', '8xs', '8xs', '8xs', '8xs', '8xs']);
        self::insertType('Program',              0x05,  ['82p', '83p', '8xp', '8xp', '8xp', '8xp', '8xp']);
        self::insertType('ProtectedProgram',     0x06,  ['82p', '83p', '8xp', '8xp', '8xp', '8xp', '8xp']);
        self::insertType('Picture',              0x07,  [ null,  null, '8xi', '8xi', '8ci', '8ci', '8ci']);
        self::insertType('GraphDataBase',        0x08,  ['82d', '83d', '8xd', '8xd', '8xd', '8xd', '8xd']);
//      self::insertType('WindowSettings',       0x0B,  ['82w', '83w', '8xw', '8xw', '8xw', '8xw', '8xw']);
        self::insertType('Complex',              0x0C,  [ null, '83c', '8xc', '8xc', '8xc', '8xc', '8xc']);
        self::insertType('ComplexList',          0x0D,  [ null, '83l', '8xl', '8xl', '8xl', '8xl', '8xl']);
        self::insertType('WindowSettings',       0x0F,  ['82w', '83w', '8xw', '8xw', '8xw', '8xw', '8xw']);
        self::insertType('RecallWindow',         0x10,  ['82z', '83z', '8xz', '8xz', '8xz', '8xz', '8xz']);
        self::insertType('TableRange',           0x11,  ['82t', '83t', '8xt', '8xt', '8xt', '8xt', '8xt']);
        self::insertType('Backup',               0x13,  ['82b', '83b',  null, '8xb', '8cb',  null,  null]);
        self::insertType('AppVar',               0x15,  [ null,  null,  null, '8xv', '8xv', '8xv', '8xv']);
        self::insertType('TemporaryItem',        0x16,  [ null,  null,  null,  null,  null,  null,  null]);
        self::insertType('GroupObject',          0x17,  ['82g', '83g', '8xg', '8xg', '8xg', '8cg', '8cg']);
        self::insertType('RealFration',          0x18,  [ null,  null,  null, '8xn', '8xn', '8xn', '8xn']);
        self::insertType('Image',                0x1A,  [ null,  null,  null,  null,  null, '8ca', '8ca']);

        /* Exact values (TI-83 Premium CE) */
        /* See https://docs.google.com/document/d/1P_OUbnZMZFg8zuOPJHAx34EnwxcQZ8HER9hPeOQ_dtI */
        self::insertType('ExactComplexFrac',     0x1B,  [ null,  null,  null,  null,  null,  null, '8xc']);
        self::insertType('ExactRealRadical',     0x1C,  [ null,  null,  null,  null,  null,  null, '8xn']);
        self::insertType('ExactComplexRadical',  0x1D,  [ null,  null,  null,  null,  null,  null, '8xc']);
        self::insertType('ExactComplexPi',       0x1E,  [ null,  null,  null,  null,  null,  null, '8xc']);
        self::insertType('ExactComplexPiFrac',   0x1F,  [ null,  null,  null,  null,  null,  null, '8xc']);
        self::insertType('ExactRealPi',          0x20,  [ null,  null,  null,  null,  null,  null, '8xn']);
        self::insertType('ExactRealPiFrac',      0x21,  [ null,  null,  null,  null,  null,  null, '8xn']);

        /* System/Flash-related things */
        self::insertType('OperatingSystem',      0x23,  ['82u', '83u', '82u', '8xu', '8cu', '8eu', '8pu']);
        self::insertType('FlashApp',             0x24,  [ null,  null,  null, '8xk', '8ck', '8ek', '8ek']);
        self::insertType('Certificate',          0x25,  [ null,  null,  null, '8xq', '8cq',  null,  null]);
        self::insertType('CertificateMemory',    0x27,  [ null,  null,  null,  null,  null,  null,  null]);
        self::insertType('Clock',                0x29,  [ null,  null,  null,  null,  null,  null,  null]);
        self::insertType('FlashLicense',         0x3E,  [ null,  null,  null,  null,  null,  null,  null]);

        // WindowSettings clone thing
        self::$types[0x0B] = self::$types[0x0F];
    }

    /**
     * @param   int     $id     The type ID
     * @return  string          The type name for that ID
     */
    public static function getNameFromID($id = -1)
    {
        if ($id !== -1 && isset(self::$types[$id]))
        {
            return self::$types[$id]['name'];
        } else {
            return 'Unknown';
        }
    }

    /**
     * @param   string  $name   The type name
     * @return  int             The type ID for that name
     */
    public static function getIDFromName($name = '')
    {
        if ($name !== '' && isset(self::$types[$name]))
        {
            return self::$types[$name]['id'];
        } else {
            return -1;
        }
    }

    /**
     * @param   int     $id     The type ID
     * @return  string[]        The array of extensions for that ID
     */
    public static function getExtensionsFromTypeID($id = -1)
    {
        if ($id !== -1 && isset(self::$types[$id]))
        {
            return self::$types[$id]['exts'];
        } else {
            return 'Unknown';
        }
    }

    /**
     * @param   string  $name
     * @return  string[]        The array of extensions for that ID
     */
    public static function getExtensionsFromName($name = '')
    {
        if ($name !== '' && isset(self::$types[$name]))
        {
            return self::$types[$name]['exts'];
        } else {
            return [];
        }
    }

    public static function isValidID($id = -1)
    {
        return ($id != -1 && is_int($id) && isset(self::$types[$id]));
    }

    public static function isValidName($name = '')
    {
        return ($name !== '' && isset(self::$types[$name]));
    }
}

TIVarTypes::initTIVarTypesArray();
