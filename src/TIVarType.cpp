
/*
 * Part of tivars_lib_cpp
 * (C) 2015 Adrien 'Adriweb' Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

namespace tivars;

use tivars\TypeHandlers\ITIVarTypeHandler;

class TIVarType
{
    private $name = 'Unknown';
    private $id   = -1;
    private $exts = [];
    private $typeHandler = null;

    /**
     * @return string
     */
    public function getName()
    {
        return $this->name;
    }

    /**
     * @return int
     */
    public function getId()
    {
        return $this->id;
    }

    /**
     * @return array
     */
    public function getExts()
    {
        return $this->exts;
    }

    /**
     * @return ITIVarTypeHandler
     */
    public function getTypeHandler()
    {
        return $this->typeHandler;
    }

    // Kind of a factory, but since the two methods in a Type Handler are static,
    // we'll just return a string being the name of the class. Later used with '::'.
    public static function determineTypeHandler($typeID)
    {
        $typeID = (int)$typeID;
        if (TIVarTypes::isValidID($typeID))
        {
            $typeID_hex = (($typeID < 0x10) ? '0' : '') . dechex($typeID);
            $handlerName = "TH_0x{$typeID_hex}";
            $handlerIncludePath = __DIR__ . "/TypeHandlers/{$handlerName}.php";
            if (file_exists($handlerIncludePath))
            {
                include_once($handlerIncludePath);
                return 'tivars\TypeHandlers\\' . $handlerName;
            } else {
                include_once('TypeHandlers/TH_Unimplemented.php');
                return 'tivars\TypeHandlers\\TH_Unimplemented';
            }
        } else {
            throw new \Exception("Invalid type ID");
        }
    }

    /*** "Constructors" ***/

    /**
     * @param   int     $id     The type ID
     * @return  TIVarType
     * @throws  \Exception
     */
    public static function createFromID($id = -1)
    {
        if (TIVarTypes::isValidID($id))
        {
            $instance = new self();
            $instance->id = $id;
            $instance->exts = TIVarTypes::getExtensionsFromTypeID($id);
            $instance->name = TIVarTypes::getNameFromID($id);
            $instance->typeHandler = self::determineTypeHandler($id);
            return $instance;
        } else {
            throw new \Exception("Invalid type ID");
        }
    }

    /**
     * @param   string  $name   The type name
     * @return  TIVarType
     * @throws  \Exception
     */
    public static function createFromName($name = '')
    {
        if (TIVarTypes::isValidName($name))
        {
            $instance = new self();
            $instance->name = $name;
            $instance->id   = TIVarTypes::getIDFromName($name);
            $instance->exts = TIVarTypes::getExtensionsFromName($name);
            $instance->typeHandler = self::determineTypeHandler($instance->id);
            return $instance;
        } else {
            throw new \Exception("Invalid type name");
        }
    }
}
