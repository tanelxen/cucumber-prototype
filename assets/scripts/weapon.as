class Weapon : MonoBehaviour
{
    [Range(1, 20), Tooltip("Выстрелов в сек")]
    int fireRate = 1;

    string effect = "Пыщ-Пыщ";

    void Fire()
    {
        print(effect);
        
        AudioComponent@ sound = GetOwner().GetAudio();

        if (sound != null)
        {
            sound.Play();
        }
    }

    void start()
    {
        print("Weapon spawned on entity: " + GetOwner().GetName());
    }

    void update(float dt)
    {
    }
}