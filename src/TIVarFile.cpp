
/*
 * Part of tivars_lib_cpp
 * (C) 2015 Adrien 'Adriweb' Bertrand
 * https://github.com/adriweb/tivars_lib_cpp
 * License: MIT
 */

namespace tivars;

date_default_timezone_set('UTC');

include_once "BinaryFile.php";

class TIVarFile extends BinaryFile
{
    private $header = [
        'signature'     => null,
        'sig_extra'     => null,
        'comment'       => null,
        'entries_len'   => null
    ];
    private $varEntry = [
        'constBytes'    => null,
        'data_length'   => null,
        'typeID'        => null,
        'varname'       => null,
        'version'       => null,
        'archivedFlag'  => null,
        'data_length2'  => null,
        'data'          => null
    ];
    /** @var TIVarType */
    private $type = null;
    /** @var TIModel */
    private $calcModel = null;
    private $computedChecksum = null;
    private $inFileChecksum = null;
    private $isFromFile = null;

    // TODO: Handle multiple varEntries


    /*** Constructors ***/

    /**
     * Internal constructor, called from loadFromFile and createNew.
     * @param   string  $filePath
     * @throws  \Exception
     */
    protected function __construct($filePath = '')
    {
        if ($filePath !== '')
        {
            $this->isFromFile = true;
            parent::__construct($filePath);
            if ($this->fileSize < 76) // bare minimum for header + a var entry
            {
                throw new \Exception("This file is not a valid TI-[e]z80 variable file");
            }
            $this->makeHeaderFromFile();
            $this->makeVarEntryFromFile();
            $this->computedChecksum = $this->computeChecksumFromFileData();
            $this->inFileChecksum = $this->getChecksumValueFromFile();
            $this->type = TIVarType::createFromID($this->varEntry['typeID']);
        } else {
            $this->isFromFile = false;
        }
    }

    public static function loadFromFile($filePath = '')
    {
        if ($filePath !== '')
        {
            return new self($filePath);
        } else {
            throw new \Exception("No file path given");
        }
    }

    public static function createNew(TIVarType $type = null, $name = '', TIModel $version = null)
    {
        if ($type !== null)
        {
            if ($name === '')
            {
                $name = 'FILE' . ((count($type->getExts()) > 0) ? $type->getExts()[0] : '');
            }
            $newName = preg_replace('/[^a-zA-Z0-9]/', '', $name);
            if ($newName !== $name || strlen($newName) > 8 || $newName === '' || is_numeric($newName[0]))
            {
                throw new \Exception("Invalid name given. 8 chars (A-Z, 0-9) max, starting by a letter");
            }
            $name = strtoupper(substr($name, 0, 8));

            $instance = new self();
            $instance->type = $type;
            $instance->calcModel = ($version !== null) ? $version : TIModel::createFromName('84+'); // default

            if (!$instance->calcModel->supportsType($instance->type))
            {
                throw new \Exception('This calculator model (' . $instance->calcModel->getName() . ') does not support the type ' . $instance->type->getName());
            }

            $instance->header = [
                'signature'     =>  $instance->calcModel->getSig(),
                'sig_extra'     =>  [ 0x1A, 0x0A, 0x00 ],
                'comment'       =>  str_pad("Created by tivars_lib_cpp on " . date("M j, Y"), 42, "\0"),
                'entries_len'   =>  0 // will have to be overwritten later
            ];
            $calcFlags = $instance->calcModel->getFlags();
            $instance->varEntry = [
                'constBytes'    =>  [ 0x0D, 0x00 ],
                'data_length'   =>  0, // will have to be overwritten later
                'typeID'        =>  $type->getId(),
                'varname'       =>  str_pad($name, 8, "\0"),
                'version'       =>  ($calcFlags >= TIFeatureFlags::hasFlash) ? 0 : null,
                'archivedFlag'  =>  ($calcFlags >= TIFeatureFlags::hasFlash) ? 0 : null, // TODO: check when that needs to be 1.
                'data_length2'  =>  0, // will have to be overwritten later
                'data'          =>  [] // will have to be overwritten later
            ];
            return $instance;
        } else {
            throw new \Exception("No file path given");
        }
    }


    /*** Makers ***/

    private function makeHeaderFromFile()
    {
        rewind($this->file);
        $this->header = [];
        $this->header['signature']   = $this->get_string_bytes(8);
        $this->header['sig_extra']   = $this->get_raw_bytes(3);
        $this->header['comment']     = $this->get_string_bytes(42);
        $this->header['entries_len'] = $this->get_raw_bytes(1)[0] + ($this->get_raw_bytes(1)[0] << 8);
        $this->calcModel = TIModel::createFromSignature($this->header['signature']);
    }

    private function makeVarEntryFromFile()
    {
        $calcFlags = $this->calcModel->getFlags();
        $dataSectionOffset = (8+3+42+2); // after header
        fseek($this->file, $dataSectionOffset);
        $this->varEntry = [];
        $this->varEntry['constBytes']   = $this->get_raw_bytes(2);
        $this->varEntry['data_length']  = $this->get_raw_bytes(1)[0] + ($this->get_raw_bytes(1)[0] << 8);
        $this->varEntry['typeID']       = $this->get_raw_bytes(1)[0];
        $this->varEntry['varname']      = $this->get_string_bytes(8);
        $this->varEntry['version']      = ($calcFlags >= TIFeatureFlags::hasFlash) ? $this->get_raw_bytes(1)[0] : null;
        $this->varEntry['archivedFlag'] = ($calcFlags >= TIFeatureFlags::hasFlash) ? $this->get_raw_bytes(1)[0] : null;
        $this->varEntry['data_length2'] = $this->get_raw_bytes(1)[0] + ($this->get_raw_bytes(1)[0] << 8);
        $this->varEntry['data']         = $this->get_raw_bytes($this->varEntry['data_length']);
    }


    /*** Getters ***/

    public function getHeader()
    {
        return $this->header;
    }

    public function getVarEntry()
    {
        return $this->varEntry;
    }

    public function getType()
    {
        return $this->type;
    }


    /*** Utils. ***/

    public function isValid()
    {
        return ($this->isFromFile) ? ($this->computedChecksum === $this->inFileChecksum)
                                   : ($this->computedChecksum !== null);
    }


    /*** Private actions ***/

    public function computeChecksumFromFileData()
    {
        if ($this->isFromFile)
        {
            $dataSectionOffset = (8 + 3 + 42 + 2); // after header
            fseek($this->file, $dataSectionOffset);
            $sum = 0;
            for ($i = $dataSectionOffset; $i < $this->fileSize - 2; $i++)
            {
                $sum += $this->get_raw_bytes(1)[0];
            }
            return $sum & 0xFFFF;
        } else {
            echo "[Error] No file loaded";
            return -1;
        }
    }

    private function computeChecksumFromInstanceData()
    {
        $sum = 0;
        $sum += array_sum($this->varEntry['constBytes']);
        $sum += 2 * (($this->varEntry['data_length'] & 0xFF) + (($this->varEntry['data_length'] >> 8) & 0xFF));
        $sum += $this->varEntry['typeID'] + (int)$this->varEntry['version'] + (int)$this->varEntry['archivedFlag'];
        $sum += array_sum(array_map('ord', str_split($this->varEntry['varname'])));
        $sum += array_sum($this->varEntry['data']);
        return $sum & 0xFFFF;
    }

    private function getChecksumValueFromFile()
    {
        if ($this->isFromFile)
        {
            fseek($this->file, $this->fileSize - 2);
            return $this->get_raw_bytes(1)[0] + ($this->get_raw_bytes(1)[0] << 8);
        } else {
            echo "[Error] No file loaded";
            return -1;
        }
    }

    /**
     *  Updates the length fields in both the header and the var entry, as well as the checksum
     */
    private function refreshMetadataFields()
    {
        $this->varEntry['data_length'] = $this->varEntry['data_length2'] = count($this->varEntry['data']);
        $this->header['entries_len'] = $this->varEntry['data_length'] + 17; // 17 == sum of the individual sizes.
        $this->computedChecksum = $this->computeChecksumFromInstanceData();
    }


    /*** Public actions **/

    /**
    * @param    array   $data   The array of bytes
    */
    public function setContentFromData(array $data = [])
    {
        if ($data !== [])
        {
            $this->varEntry['data'] = $data;
            $this->refreshMetadataFields();
        } else {
            echo "[Error] No data given";
        }
    }

    public function setContentFromString($str = '', $options = [])
    {
        $handler = $this->type->getTypeHandler();
        $this->varEntry['data'] = $handler::makeDataFromString($str, $options);
        $this->refreshMetadataFields();
    }

    public function getRawContent()
    {
        return $this->varEntry['data'];
    }

    public function getReadableContent($options = [])
    {
        $handler = $this->type->getTypeHandler();
        return $handler::makeStringFromData($this->varEntry['data'], $options);
    }

    public function fixChecksumInFile()
    {
        if ($this->isFromFile)
        {
            if (!$this->isValid())
            {
                fseek($this->file, $this->fileSize - 2);
                fwrite($this->file, chr($this->computedChecksum & 0xFF) . chr(($this->computedChecksum >> 8) & 0xFF));
                $this->inFileChecksum = $this->getChecksumValueFromFile();
            }
        } else {
            echo "[Error] No file loaded";
        }
    }

    /**
     * Writes a variable to an actual file on the FS
     * If the variable was already loaded from a file, it will be used and overwritten,
     * except if a specific directory and name are provided.
     *
     * @param   string  $directory  Directory to save the file to
     * @param   string  $name       Name of the file, without the extension
     */
    public function saveVarToFile($directory = '', $name = '')
    {
        if ($this->isFromFile && $directory === '')
        {
            $this->close();
            $handle = fopen($this->filePath, 'wb');
        } else {
            if ($name === '')
            {
                $name = $this->varEntry['varname'];
            }
            // TODO: make user be able to precise for which model the extension will be fitted
            $fileName = str_replace("\0", '', $name) . '.' . $this->getType()->getExts()[0];
            if ($directory === '')
            {
                $directory = './';
            }
            $directory = rtrim($directory, '/');
            $fullPath = realpath($directory) . '/' . $fileName;
            $handle = fopen($fullPath, 'wb');
        }

        $this->refreshMetadataFields();

        $bin_data = '';
        foreach ([$this->header, $this->varEntry] as $whichData)
        {
            foreach ($whichData as $key => $data)
            {
                // fields not used for this calc version, for instance.
                if ($data === null)
                {
                    continue;
                }
                switch (gettype($data))
                {
                    case 'integer':
                        // The length fields are the only ones on 2 bytes.
                        if ($key === "entries_len" || $key === "data_length" || $key === "data_length2")
                        {
                            $bin_data .= chr($data & 0xFF) . chr(($data >> 8) & 0xFF);
                        } else {
                            $bin_data .= chr($data & 0xFF);
                        }
                        break;
                    case 'string':
                        $bin_data .= $data;
                        break;
                    case 'array':
                        foreach ($data as $subData)
                        {
                            $bin_data .= chr($subData & 0xFF);
                        }
                        break;
                }
            }
        }

        fwrite($handle, $bin_data);
        fwrite($handle, chr($this->computedChecksum & 0xFF) . chr(($this->computedChecksum >> 8) & 0xFF));

        fclose($handle);
    }

}